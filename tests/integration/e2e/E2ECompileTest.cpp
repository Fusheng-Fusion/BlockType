//===--- E2ECompileTest.cpp - End-to-End Compilation Tests -*- C++ -*-===//
//
// End-to-end integration tests that exercise the full pipeline:
//   Source → Lexer → Parser → Sema → AST → IR (BTIR) → LLVM IR → Object File
//
// These tests write temporary source files, invoke CompilerInstance to compile
// them through the new pluggable pipeline (Frontend→IR→Backend), and verify
// that the output object files are produced correctly.
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>

#include "blocktype/Frontend/CompilerInstance.h"
#include "blocktype/Frontend/CompilerInvocation.h"
#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/Backend/LLVMBackend.h"
#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/Frontend/CppFrontend.h"
#include "blocktype/Basic/Diagnostics.h"

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Error.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Object/ObjectFile.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace blocktype;

namespace {
// Initialize LLVM targets and register frontends/backends once.
struct LLVMTargetInit {
  LLVMTargetInit() {
    // Initialize LLVM target backends
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();

    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();

    // Explicitly register frontends and backends (static registrators may be
    // optimized away by the linker when the test executable doesn't reference
    // them directly).
    auto& FEReg = frontend::FrontendRegistry::instance();
    if (!FEReg.hasFrontend("cpp")) {
      FEReg.registerFrontend("cpp",
        [](const frontend::FrontendCompileOptions& Opts,
           DiagnosticsEngine& Diags) -> std::unique_ptr<frontend::FrontendBase> {
          return std::make_unique<frontend::CppFrontend>(Opts, Diags);
        });
      FEReg.addExtensionMapping(".cpp", "cpp");
      FEReg.addExtensionMapping(".cc",  "cpp");
      FEReg.addExtensionMapping(".cxx", "cpp");
      FEReg.addExtensionMapping(".C",   "cpp");
      FEReg.addExtensionMapping(".c",   "cpp");
    }

    auto& BEReg = backend::BackendRegistry::instance();
    if (!BEReg.hasBackend("llvm")) {
      BEReg.registerBackend("llvm", backend::createLLVMBackend);
    }
  }
};
static LLVMTargetInit TargetInit;

/// Helper: create a temporary source file with the given content and extension.
/// Returns the file path. Caller is responsible for cleanup.
std::string writeTempSource(llvm::StringRef Content,
                             llvm::StringRef Ext = ".cpp") {
  char TempPath[] = "/tmp/blocktype-e2e-XXXXXX";
  int FD = mkstemp(TempPath);
  if (FD == -1)
    return "";
  write(FD, Content.data(), Content.size());
  close(FD);

  // Rename to have correct extension
  std::string Path(TempPath);
  std::string NewPath = Path + Ext.str();
  rename(Path.c_str(), NewPath.c_str());
  return NewPath;
}

/// Helper: remove a file if it exists.
void cleanupFile(const std::string &Path) {
  if (!Path.empty())
    unlink(Path.c_str());
}

/// Helper: check if a file exists and is non-empty.
bool fileExistsAndNonEmpty(const std::string &Path) {
  if (Path.empty())
    return false;
  auto BufOrErr = llvm::MemoryBuffer::getFile(Path);
  if (!BufOrErr)
    return false;
  return (*BufOrErr)->getBufferSize() > 0;
}

/// Helper: parse an object file and extract symbol names.
/// Includes both exported symbols and section symbols.
std::vector<std::string> getObjectFileSymbols(const std::string &Path) {
  std::vector<std::string> Symbols;
  auto BufOrErr = llvm::MemoryBuffer::getFile(Path);
  if (!BufOrErr)
    return Symbols;

  auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
      (*BufOrErr)->getMemBufferRef());
  if (!ObjOrErr)
    return Symbols;

  // Collect named symbols
  for (auto &Sym : (*ObjOrErr)->symbols()) {
    auto NameOrErr = Sym.getName();
    if (NameOrErr && !NameOrErr->empty()) {
      std::string N = NameOrErr->str();
      uint32_t Flags = cantFail(Sym.getFlags());
      if (Flags & llvm::object::SymbolRef::SF_Undefined)
        continue;
      // On macOS, strip the leading underscore from C symbols
      if (N.size() > 1 && N[0] == '_')
        N = N.substr(1);
      Symbols.push_back(N);
    }
  }

  // On Mach-O, local functions may only appear as section-level symbols.
  // Also check export trie or indirect symbols for completeness.
  // For now, also verify the file is a valid object file by checking sections.
  return Symbols;
}

/// Helper: verify a file is a valid object file.
bool isValidObjectFile(const std::string &Path) {
  auto BufOrErr = llvm::MemoryBuffer::getFile(Path);
  if (!BufOrErr)
    return false;
  auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
      (*BufOrErr)->getMemBufferRef());
  return static_cast<bool>(ObjOrErr);
}

/// Helper: count sections in an object file (validates code was emitted).
unsigned countObjectSections(const std::string &Path) {
  auto BufOrErr = llvm::MemoryBuffer::getFile(Path);
  if (!BufOrErr)
    return 0;
  auto ObjOrErr = llvm::object::ObjectFile::createObjectFile(
      (*BufOrErr)->getMemBufferRef());
  if (!ObjOrErr)
    return 0;
  unsigned Count = 0;
  for (auto &Sec : (*ObjOrErr)->sections())
    ++Count;
  return Count;
}

} // anonymous namespace

// ============================================================================
// Test Fixture
// ============================================================================

class E2ECompileTest : public ::testing::Test {
protected:
  std::vector<std::string> TempFiles;

  void TearDown() override {
    for (auto &F : TempFiles) {
      cleanupFile(F);
      cleanupFile(F + ".o");
    }
  }

  /// Create a temp source file, track it for cleanup, and return its path.
  std::string createSource(llvm::StringRef Content,
                            llvm::StringRef Ext = ".cpp") {
    std::string Path = writeTempSource(Content, Ext);
    TempFiles.push_back(Path);
    return Path;
  }

  /// Compile a source file through the full pipeline and return success.
  bool compileToObj(llvm::StringRef Source,
                    const std::string &TargetTriple = "",
                    unsigned OptLevel = 0) {
    std::string SrcPath = createSource(Source);
    std::string ObjPath = SrcPath + ".o";

    auto CI = std::make_shared<CompilerInvocation>();
    CI->FrontendOpts.InputFiles = {SrcPath};
    CI->CodeGenOpts.OutputFile = ObjPath;
    CI->CodeGenOpts.EmitObject = true;
    CI->CodeGenOpts.OptimizationLevel = OptLevel;
    if (!TargetTriple.empty())
      CI->TargetOpts.Triple = TargetTriple;
    CI->setDefaultTargetTriple();

    CompilerInstance Instance;
    if (!Instance.initialize(CI))
      return false;
    return Instance.compileAllFiles();
  }

  /// Compile to object file and return the object file path.
  /// Returns empty string on failure.
  std::string compileAndGetObjPath(llvm::StringRef Source,
                                    const std::string &TargetTriple = "") {
    std::string SrcPath = createSource(Source);
    std::string ObjPath = SrcPath + ".o";

    auto CI = std::make_shared<CompilerInvocation>();
    CI->FrontendOpts.InputFiles = {SrcPath};
    CI->CodeGenOpts.OutputFile = ObjPath;
    CI->CodeGenOpts.EmitObject = true;
    if (!TargetTriple.empty())
      CI->TargetOpts.Triple = TargetTriple;
    CI->setDefaultTargetTriple();

    CompilerInstance Instance;
    if (!Instance.initialize(CI))
      return "";
    if (!Instance.compileAllFiles())
      return "";
    return ObjPath;
  }
};

// ============================================================================
// 1. Basic Function Compilation
// ============================================================================

TEST_F(E2ECompileTest, EmptyFunction) {
  const char *Source = R"(
void empty() {}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty()) << "Compilation failed for empty function";
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

TEST_F(E2ECompileTest, ReturnInteger) {
  const char *Source = R"(
int ret42() { return 42; }
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty()) << "Compilation failed for return integer";
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
  EXPECT_TRUE(isValidObjectFile(ObjPath));
  // Verify code sections exist (text, data, etc.)
  EXPECT_GE(countObjectSections(ObjPath), 1u);
}

TEST_F(E2ECompileTest, SimpleArithmetic) {
  const char *Source = R"(
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int sub(int a, int b) { return a - b; }
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty()) << "Compilation failed for arithmetic";
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
  EXPECT_TRUE(isValidObjectFile(ObjPath));
}

// ============================================================================
// 2. Variable Declarations and Types
// ============================================================================

TEST_F(E2ECompileTest, LocalVariables) {
  const char *Source = R"(
int test_vars() {
  int x = 10;
  int y = 20;
  int z = x + y;
  return z;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

TEST_F(E2ECompileTest, BasicTypes) {
  const char *Source = R"(
int test_int() { int x = 42; return x; }
long test_long() { long x = 100; return x; }
float test_float() { float x = 3.14; return x; }
double test_double() { double x = 2.718; return x; }
char test_char() { char c = 65; return c; }
bool test_bool() { bool b = true; return b; }
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

// ============================================================================
// 3. Control Flow
// ============================================================================

TEST_F(E2ECompileTest, IfElse) {
  const char *Source = R"(
int abs_val(int x) {
  if (x < 0) {
    return -x;
  } else {
    return x;
  }
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, ForLoop) {
  const char *Source = R"(
int sum_to_n(int n) {
  int sum = 0;
  for (int i = 0; i < n; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, WhileLoop) {
  const char *Source = R"(
int while_sum(int n) {
  int sum = 0;
  int i = 0;
  while (i < n) {
    sum = sum + i;
    i = i + 1;
  }
  return sum;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, NestedControlFlow) {
  const char *Source = R"(
int nested(int n) {
  int count = 0;
  for (int i = 0; i < n; i = i + 1) {
    if (i > 5) {
      count = count + 1;
    }
  }
  return count;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

// ============================================================================
// 4. Functions and Calls
// ============================================================================

TEST_F(E2ECompileTest, FunctionCall) {
  const char *Source = R"(
int square(int x) { return x * x; }

int sum_of_squares(int a, int b) {
  return square(a) + square(b);
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(isValidObjectFile(ObjPath));
}

TEST_F(E2ECompileTest, RecursiveFunction) {
  const char *Source = R"(
int fib(int n) {
  if (n <= 1) return n;
  return fib(n - 1) + fib(n - 2);
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, MultipleFunctions) {
  const char *Source = R"(
int identity(int x) { return x; }
int add(int a, int b) { return a + b; }
int negate(int x) { return -x; }

int dispatch(int op, int a, int b) {
  if (op == 0) return identity(a);
  if (op == 1) return add(a, b);
  return negate(a);
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(isValidObjectFile(ObjPath));
  // Verify the object file has multiple sections (code + data)
  EXPECT_GE(countObjectSections(ObjPath), 1u);
}

// ============================================================================
// 5. Classes and Structs
// ============================================================================

TEST_F(E2ECompileTest, SimpleStruct) {
  const char *Source = R"(
struct Point {
  int x;
  int y;
};

int point_sum(struct Point p) {
  return p.x + p.y;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, ClassWithMethods) {
  const char *Source = R"(
class Counter {
  int val;
public:
  int get() { return val; }
  void set(int v) { val = v; }
};

int test_counter() {
  Counter c;
  c.set(42);
  return c.get();
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, SimpleInheritance) {
  const char *Source = R"(
class Base {
public:
  int base_val;
  int get_base() { return base_val; }
};

class Derived : public Base {
public:
  int derived_val;
  int get_derived() { return derived_val; }
};

int test_inheritance() {
  Derived d;
  d.base_val = 10;
  d.derived_val = 20;
  return d.get_base() + d.get_derived();
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, VirtualDispatch) {
  const char *Source = R"(
class Shape {
public:
  virtual int area() { return 0; }
};

class Rect : public Shape {
public:
  int w;
  int h;
  int area() override { return w * h; }
};

int test_virtual() {
  Rect r;
  r.w = 3;
  r.h = 4;
  return r.area();
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

// ============================================================================
// 6. Pointers and Arrays
// ============================================================================

TEST_F(E2ECompileTest, PointerAccess) {
  const char *Source = R"(
int deref(int *p) { return *p; }
int assign_ptr(int *p, int v) { *p = v; return 0; }
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, ArrayAccess) {
  const char *Source = R"(
int sum_array(int *arr, int n) {
  int sum = 0;
  for (int i = 0; i < n; i = i + 1) {
    sum = sum + arr[i];
  }
  return sum;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

// ============================================================================
// 7. Cross-Platform Targets
// ============================================================================

TEST_F(E2ECompileTest, X86Target) {
  const char *Source = R"(
int test_x86() { return 86; }
)";
  std::string ObjPath = compileAndGetObjPath(Source, "x86_64-unknown-linux-gnu");
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

TEST_F(E2ECompileTest, AArch64Target) {
  const char *Source = R"(
int test_aarch64() { return 64; }
)";
  std::string ObjPath = compileAndGetObjPath(Source, "aarch64-unknown-linux-gnu");
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

TEST_F(E2ECompileTest, NativeTarget) {
  const char *Source = R"(
int test_native() { return 1; }
)";
  // Empty target triple triggers default (native)
  std::string ObjPath = compileAndGetObjPath(Source, "");
  ASSERT_FALSE(ObjPath.empty());
  EXPECT_TRUE(fileExistsAndNonEmpty(ObjPath));
}

// ============================================================================
// 8. Optimization Levels
// ============================================================================

TEST_F(E2ECompileTest, OptimizationO0) {
  const char *Source = R"(
int compute(int n) {
  int sum = 0;
  for (int i = 0; i < n; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
)";
  EXPECT_TRUE(compileToObj(Source, "", 0));
}

TEST_F(E2ECompileTest, OptimizationO2) {
  const char *Source = R"(
int compute(int n) {
  int sum = 0;
  for (int i = 0; i < n; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
)";
  EXPECT_TRUE(compileToObj(Source, "", 2));
}

TEST_F(E2ECompileTest, OptimizationO3) {
  const char *Source = R"(
int compute(int n) {
  int sum = 0;
  for (int i = 0; i < n; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
)";
  EXPECT_TRUE(compileToObj(Source, "", 3));
}

// ============================================================================
// 9. Emit LLVM IR Mode
// ============================================================================

TEST_F(E2ECompileTest, EmitLLVMIR) {
  const char *Source = R"(
int emit_ir_test(int x) { return x + 1; }
)";
  std::string SrcPath = createSource(Source);
  std::string ObjPath = SrcPath + ".o";

  auto CI = std::make_shared<CompilerInvocation>();
  CI->FrontendOpts.InputFiles = {SrcPath};
  CI->CodeGenOpts.OutputFile = ObjPath;
  CI->CodeGenOpts.EmitLLVM = true;
  CI->setDefaultTargetTriple();

  CompilerInstance Instance;
  ASSERT_TRUE(Instance.initialize(CI));
  EXPECT_TRUE(Instance.compileAllFiles());
}

// ============================================================================
// 10. Multiple Source Files
// ============================================================================

TEST_F(E2ECompileTest, MultipleFilesCompilation) {
  // Create two independent source files
  std::string Src1 = createSource(R"(
int func_a() { return 1; }
)");
  std::string Src2 = createSource(R"(
int func_b() { return 2; }
)");

  // Compile first file
  std::string Obj1 = Src1 + ".o";
  {
    auto CI = std::make_shared<CompilerInvocation>();
    CI->FrontendOpts.InputFiles = {Src1};
    CI->CodeGenOpts.OutputFile = Obj1;
    CI->CodeGenOpts.EmitObject = true;
    CI->setDefaultTargetTriple();

    CompilerInstance Inst;
    ASSERT_TRUE(Inst.initialize(CI));
    EXPECT_TRUE(Inst.compileAllFiles());
  }
  EXPECT_TRUE(fileExistsAndNonEmpty(Obj1));

  // Compile second file
  std::string Obj2 = Src2 + ".o";
  {
    auto CI = std::make_shared<CompilerInvocation>();
    CI->FrontendOpts.InputFiles = {Src2};
    CI->CodeGenOpts.OutputFile = Obj2;
    CI->CodeGenOpts.EmitObject = true;
    CI->setDefaultTargetTriple();

    CompilerInstance Inst;
    ASSERT_TRUE(Inst.initialize(CI));
    EXPECT_TRUE(Inst.compileAllFiles());
  }
  EXPECT_TRUE(fileExistsAndNonEmpty(Obj2));

  // Verify both object files are valid
  EXPECT_TRUE(isValidObjectFile(Obj1));
  EXPECT_TRUE(isValidObjectFile(Obj2));
}

// ============================================================================
// 11. Expression Complexity
// ============================================================================

TEST_F(E2ECompileTest, ComplexExpressions) {
  const char *Source = R"(
int complex_expr(int a, int b, int c) {
  int x = (a + b) * c - a / b;
  int y = x + (c - a) * (b + 1);
  int z = y - x / (a + 1);
  return z;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, ComparisonAndLogical) {
  const char *Source = R"(
int logic_test(int a, int b) {
  if (a > 0 && b > 0) return 1;
  if (a < 0 || b < 0) return -1;
  return 0;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

// ============================================================================
// 12. Namespaces and Enums
// ============================================================================

TEST_F(E2ECompileTest, NamespaceFunction) {
  const char *Source = R"(
namespace math {
  int add(int a, int b) { return a + b; }
}

int use_math() { return math::add(1, 2); }
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

TEST_F(E2ECompileTest, EnumUsage) {
  const char *Source = R"(
enum Color { Red, Green, Blue };

int color_val(Color c) {
  if (c == Red) return 0;
  if (c == Green) return 1;
  return 2;
}
)";
  std::string ObjPath = compileAndGetObjPath(Source);
  ASSERT_FALSE(ObjPath.empty());
}

// ============================================================================
// 13. Pluggable Pipeline Verification
// ============================================================================

TEST_F(E2ECompileTest, PipelineUsesRegisteredFrontend) {
  // Verify the 'cpp' frontend is registered and used
  EXPECT_TRUE(frontend::FrontendRegistry::instance().hasFrontend("cpp"));
}

TEST_F(E2ECompileTest, PipelineUsesRegisteredBackend) {
  // Verify the 'llvm' backend is registered and used
  EXPECT_TRUE(backend::BackendRegistry::instance().hasBackend("llvm"));
}

TEST_F(E2ECompileTest, InvalidFrontendFails) {
  std::string SrcPath = createSource("int x = 1;");
  std::string ObjPath = SrcPath + ".o";

  auto CI = std::make_shared<CompilerInvocation>();
  CI->FrontendOpts.InputFiles = {SrcPath};
  CI->CodeGenOpts.OutputFile = ObjPath;
  CI->setDefaultTargetTriple();
  CI->setFrontendName("nonexistent");

  CompilerInstance Instance;
  ASSERT_TRUE(Instance.initialize(CI));
  EXPECT_FALSE(Instance.compileAllFiles())
      << "Should fail with nonexistent frontend";
}

TEST_F(E2ECompileTest, InvalidBackendFails) {
  std::string SrcPath = createSource("int x = 1;");
  std::string ObjPath = SrcPath + ".o";

  auto CI = std::make_shared<CompilerInvocation>();
  CI->FrontendOpts.InputFiles = {SrcPath};
  CI->CodeGenOpts.OutputFile = ObjPath;
  CI->setDefaultTargetTriple();
  CI->setBackendName("nonexistent");

  CompilerInstance Instance;
  ASSERT_TRUE(Instance.initialize(CI));
  EXPECT_FALSE(Instance.compileAllFiles())
      << "Should fail with nonexistent backend";
}
