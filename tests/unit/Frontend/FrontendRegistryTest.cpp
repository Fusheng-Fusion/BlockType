//===--- FrontendRegistryTest.cpp - FrontendRegistry tests ----*- C++ -*-===//

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/FrontendBase.h"
#include "blocktype/Frontend/FrontendCompileOptions.h"
#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/TargetLayout.h"

using namespace blocktype;
using namespace blocktype::frontend;
using namespace blocktype::ir;

// ============================================================
// Test helper: A concrete frontend for registry testing
// ============================================================

class MockCppFrontend : public FrontendBase {
public:
  using FrontendBase::FrontendBase;

  ir::StringRef getName() const override { return "cpp"; }
  ir::StringRef getLanguage() const override { return "c++"; }

  std::unique_ptr<ir::IRModule> compile(
      ir::StringRef Filename,
      ir::IRTypeContext& TypeCtx,
      const ir::TargetLayout& Layout) override {
    return std::make_unique<ir::IRModule>(Filename, TypeCtx,
                                           Opts_.TargetTriple);
  }

  bool canHandle(ir::StringRef Filename) const override {
    return Filename.endswith(".cpp") || Filename.endswith(".cc") ||
           Filename.endswith(".cxx") || Filename.endswith(".c");
  }
};

class MockBtFrontend : public FrontendBase {
public:
  using FrontendBase::FrontendBase;

  ir::StringRef getName() const override { return "bt"; }
  ir::StringRef getLanguage() const override { return "blocktype"; }

  std::unique_ptr<ir::IRModule> compile(
      ir::StringRef Filename,
      ir::IRTypeContext& TypeCtx,
      const ir::TargetLayout& Layout) override {
    return std::make_unique<ir::IRModule>(Filename, TypeCtx,
                                           Opts_.TargetTriple);
  }

  bool canHandle(ir::StringRef Filename) const override {
    return Filename.endswith(".bt");
  }
};

// Factory functions
static std::unique_ptr<FrontendBase>
createCppFrontend(const FrontendCompileOptions& Opts,
                  DiagnosticsEngine& Diags) {
  return std::make_unique<MockCppFrontend>(Opts, Diags);
}

static std::unique_ptr<FrontendBase>
createBtFrontend(const FrontendCompileOptions& Opts,
                 DiagnosticsEngine& Diags) {
  return std::make_unique<MockBtFrontend>(Opts, Diags);
}

// Unique names to avoid collision when tests share the singleton.
static const char* CPP_NAME = "regtest_cpp";
static const char* BT_NAME  = "regtest_bt";

// ============================================================
// Test cases (each self-contained for gtest_discover_tests)
// ============================================================

/// Singleton identity: instance() returns the same object every time.
TEST(FrontendRegistryTest, SingletonIdentity) {
  auto& A = FrontendRegistry::instance();
  auto& B = FrontendRegistry::instance();
  EXPECT_EQ(&A, &B);
}

/// Register a frontend and create an instance by name.
TEST(FrontendRegistryTest, RegisterAndCreate) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(CPP_NAME))
    Reg.registerFrontend(CPP_NAME, createCppFrontend);

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = Reg.create(CPP_NAME, Opts, Diags);
  ASSERT_NE(FE, nullptr);
  EXPECT_EQ(FE->getName(), "cpp");
  EXPECT_EQ(FE->getLanguage(), "c++");
}

/// Auto-select frontend by file extension.
TEST(FrontendRegistryTest, AutoSelect) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(BT_NAME))
    Reg.registerFrontend(BT_NAME, createBtFrontend);
  Reg.addExtensionMapping(".bt", BT_NAME);

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = Reg.autoSelect("hello.bt", Opts, Diags);
  ASSERT_NE(FE, nullptr);
  EXPECT_EQ(FE->getName(), "bt");
}

/// Auto-select works with full paths.
TEST(FrontendRegistryTest, AutoSelectWithPath) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(BT_NAME))
    Reg.registerFrontend(BT_NAME, createBtFrontend);
  Reg.addExtensionMapping(".bt", BT_NAME);

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = Reg.autoSelect("/path/to/source/hello.bt", Opts, Diags);
  ASSERT_NE(FE, nullptr);
  EXPECT_EQ(FE->getName(), "bt");
}

/// Creating an unknown frontend returns nullptr.
TEST(FrontendRegistryTest, CreateUnknownReturnsNullptr) {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.create("nonexistent_frontend_b2", Opts, Diags);
  EXPECT_EQ(FE, nullptr);
}

/// Auto-selecting an unknown extension returns nullptr.
TEST(FrontendRegistryTest, AutoSelectUnknownExtensionReturnsNullptr) {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.autoSelect("unknown_b2.xyz", Opts, Diags);
  EXPECT_EQ(FE, nullptr);
}

/// Auto-selecting a file with no extension returns nullptr.
TEST(FrontendRegistryTest, AutoSelectNoExtensionReturnsNullptr) {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.autoSelect("no_extension_file_b2", Opts, Diags);
  EXPECT_EQ(FE, nullptr);
}

/// hasFrontend query works correctly.
TEST(FrontendRegistryTest, HasFrontend) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(CPP_NAME))
    Reg.registerFrontend(CPP_NAME, createCppFrontend);
  if (!Reg.hasFrontend(BT_NAME))
    Reg.registerFrontend(BT_NAME, createBtFrontend);

  EXPECT_TRUE(Reg.hasFrontend(CPP_NAME));
  EXPECT_TRUE(Reg.hasFrontend(BT_NAME));
  EXPECT_FALSE(Reg.hasFrontend("nonexistent_b2"));
}

/// getRegisteredNames returns all registered names.
TEST(FrontendRegistryTest, GetRegisteredNames) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(CPP_NAME))
    Reg.registerFrontend(CPP_NAME, createCppFrontend);
  if (!Reg.hasFrontend(BT_NAME))
    Reg.registerFrontend(BT_NAME, createBtFrontend);

  auto Names = Reg.getRegisteredNames();
  EXPECT_GE(Names.size(), 2u);
  bool hasCpp = false, hasBt = false;
  for (const auto& N : Names) {
    if (N == CPP_NAME) hasCpp = true;
    if (N == BT_NAME) hasBt = true;
  }
  EXPECT_TRUE(hasCpp);
  EXPECT_TRUE(hasBt);
}

/// Extension mapping works for multiple extensions to the same frontend.
TEST(FrontendRegistryTest, ExtensionMapping) {
  auto& Reg = FrontendRegistry::instance();
  if (!Reg.hasFrontend(CPP_NAME))
    Reg.registerFrontend(CPP_NAME, createCppFrontend);
  Reg.addExtensionMapping(".cpp_b2", CPP_NAME);
  Reg.addExtensionMapping(".cc_b2", CPP_NAME);
  Reg.addExtensionMapping(".cxx_b2", CPP_NAME);

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE1 = Reg.autoSelect("test.cpp_b2", Opts, Diags);
  ASSERT_NE(FE1, nullptr);
  EXPECT_EQ(FE1->getName(), "cpp");

  auto FE2 = Reg.autoSelect("test.cc_b2", Opts, Diags);
  ASSERT_NE(FE2, nullptr);
  EXPECT_EQ(FE2->getName(), "cpp");

  auto FE3 = Reg.autoSelect("test.cxx_b2", Opts, Diags);
  ASSERT_NE(FE3, nullptr);
  EXPECT_EQ(FE3->getName(), "cpp");
}
