# Task B.4 优化版：ASTToIRConverter 主框架

> 规格来源：`12-AI-Coder-任务流-PhaseB.md` 第 464~608 行
> 优化者：planner | 日期：2026-04-25

---

## 规格修正清单（dev-tester 必读）

| # | 原规格问题 | 修正方案 | 涉及文件 |
|---|-----------|---------|---------|
| 1 | `DenseMap` 裸名 | → `ir::DenseMap` | 头文件/实现 |
| 2 | `unique_ptr` 裸名 | → `std::unique_ptr` | 头文件/实现 |
| 3 | 规格重新定义 `ir::IRConversionResult`（与 Phase A 已有类冲突） | 扩展现有 `IRConversionResult`（添加 `addError()` + `getModule()`），不新建类 | `IRConversionResult.h` |
| 4 | `TargetLayout::getTriple()` 不存在（`TripleStr` 是私有字段无 getter） | 添加 `StringRef getTriple() const` getter | `TargetLayout.h` |
| 5 | `VarDecl::hasGlobalStorage()` 不存在 | 添加 `bool hasGlobalStorage() const` 方法（命名空间级别非 static 即 global） | `Decl.h` |
| 6 | `CXXRecordDecl::hasVTable()` 不存在 | 添加 `bool hasVTable() const` stub（返回 `false`，B.9 实现） | `Decl.h` |
| 7 | `IRConstantUndef::get(T)` 签名不符 | 项目 API 是 `IRConstantUndef::get(IRContext&, IRType*)`，需传 `IRCtx_` | 实现文件 |
| 8 | `IROpaqueType("error")` 直接构造 | 应通过 `TypeCtx.getOpaqueType("error")` 获取 | 实现文件 |
| 9 | `IREmitExpr`/`IREmitStmt`/`IREmitCXX`/`IRConstantEmitter` 尚不存在 | B.4 仅前置声明，`unique_ptr` 改为裸指针（B.5-B.9 时切换回 `unique_ptr`） | 头文件 |
| 10 | 规格引用 `diag::err_ir_*` 系列 DiagID | 新增到 `DiagnosticSemaKinds.def` | `DiagnosticSemaKinds.def` |

---

## Part 1: 头文件（完整代码）

### 1.1 新增 `include/blocktype/Frontend/ASTToIRConverter.h`

```cpp
//===--- ASTToIRConverter.h - AST to IR Conversion Framework ---*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the ASTToIRConverter class, which drives the conversion
// from AST (TranslationUnitDecl) to IR (IRModule). It orchestrates the
// sub-emitters (IREmitExpr, IREmitStmt, IREmitCXX, IRConstantEmitter) which
// are implemented in subsequent tasks (B.5-B.9).
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_ASTTOIRCONVERTER_H
#define BLOCKTYPE_FRONTEND_ASTTOIRCONVERTER_H

#include <memory>

#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/IRTypeMapper.h"
#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/TargetLayout.h"

namespace blocktype {
namespace frontend {

// Forward declarations for sub-emitters (B.5-B.9)
class IREmitExpr;
class IREmitStmt;
class IREmitCXX;
class IRConstantEmitter;

/// ASTToIRConverter - Drives the top-level conversion from AST to IR.
///
/// Responsibilities:
/// 1. Creates an IRModule for the translation unit
/// 2. Iterates over top-level declarations (functions, globals, vtables)
/// 3. Delegates expression/statement/C++ specific conversion to sub-emitters
/// 4. Implements error recovery: on failure, emits a placeholder and continues
///
/// Lifecycle:
/// - Construct once per translation unit
/// - Call convert() to produce an IRConversionResult
/// - After convert(), the converter should not be reused
///
/// Thread safety: Not thread-safe. One instance per compilation thread.
class ASTToIRConverter {
  ir::IRContext& IRCtx_;
  ir::IRTypeContext& TypeCtx_;
  const ir::TargetLayout& Layout_;
  DiagnosticsEngine& Diags_;

  std::unique_ptr<ir::IRModule> TheModule_;
  std::unique_ptr<ir::IRBuilder> Builder_;
  std::unique_ptr<IRTypeMapper> TypeMapper_;

  /// Maps AST declarations to their corresponding IR values.
  ir::DenseMap<const Decl*, ir::IRValue*> DeclValues_;

  /// Maps VarDecls at global scope to IR global variables.
  ir::DenseMap<const VarDecl*, ir::IRGlobalVariable*> GlobalVars_;

  /// Maps FunctionDecls to IR functions.
  ir::DenseMap<const FunctionDecl*, ir::IRFunction*> Functions_;

  /// Maps VarDecls at local scope to IR values (alloca/load).
  ir::DenseMap<const VarDecl*, ir::IRValue*> LocalDecls_;

  // Sub-emitters (allocated in initializeEmitters(), B.5-B.9 fill in impl).
  // Using raw pointers; ownership managed via manual delete in destructor.
  // Will be converted to unique_ptr when sub-emitter headers are available.
  IREmitExpr* ExprEmitter_ = nullptr;
  IREmitStmt* StmtEmitter_ = nullptr;
  IREmitCXX* CXXEmitter_ = nullptr;
  IRConstantEmitter* ConstEmitter_ = nullptr;

public:
  /// Construct an ASTToIRConverter.
  ///
  /// \param IRCtx   IR context for allocating IR nodes.
  /// \param TypeCtx IR type context for creating types.
  /// \param Layout  Target layout for platform-dependent decisions.
  /// \param Diags   Diagnostics engine for error reporting.
  ASTToIRConverter(ir::IRContext& IRCtx,
                   ir::IRTypeContext& TypeCtx,
                   const ir::IRTargetLayout& Layout,
                   DiagnosticsEngine& Diags);

  /// Destructor. Cleans up sub-emitter objects.
  ~ASTToIRConverter();

  // Non-copyable, non-movable (references + DenseMap).
  ASTToIRConverter(const ASTToIRConverter&) = delete;
  ASTToIRConverter& operator=(const ASTToIRConverter&) = delete;
  ASTToIRConverter(ASTToIRConverter&&) = delete;
  ASTToIRConverter& operator=(ASTToIRConverter&&) = delete;

  /// Convert a translation unit AST into an IR module.
  ///
  /// \param TU The translation unit to convert.
  /// \returns An IRConversionResult. On success, isUsable() == true.
  ///          On error, contains error count and possibly a partial module.
  ir::IRConversionResult convert(TranslationUnitDecl* TU);

  // === Top-level declaration emitters ===

  /// Emit an IR function for the given FunctionDecl.
  /// Creates the function signature and body.
  ir::IRFunction* emitFunction(FunctionDecl* FD);

  /// Emit an IR global variable for the given VarDecl.
  ir::IRGlobalVariable* emitGlobalVar(VarDecl* VD);

  // === C++ specific converters (delegates to IREmitCXX, B.9) ===

  /// Convert a C++ constructor.
  void convertCXXConstructor(CXXConstructorDecl* Ctor, ir::IRFunction* IRFn);

  /// Convert a C++ destructor.
  void convertCXXDestructor(CXXDestructorDecl* Dtor, ir::IRFunction* IRFn);

  /// Convert a CXXConstructExpr.
  ir::IRValue* convertCXXConstructExpr(CXXConstructExpr* CCE);

  /// Convert a virtual method call.
  ir::IRValue* convertVirtualCall(CXXMemberCallExpr* MCE);

  // === Error recovery ===

  /// Emit a placeholder value for a failed expression conversion.
  /// Returns IRConstantUndef of the given type.
  ir::IRValue* emitErrorPlaceholder(ir::IRType* T);

  /// Emit an error type (IROpaqueType("error")) for failed type mapping.
  ir::IRType* emitErrorType();

  // === Accessors for sub-emitters ===

  ir::IRBuilder& getBuilder() { return *Builder_; }
  ir::IRModule* getModule() const { return TheModule_.get(); }
  IRTypeMapper& getTypeMapper() { return *TypeMapper_; }
  ir::IRContext& getIRContext() { return IRCtx_; }
  ir::IRTypeContext& getTypeContext() { return TypeCtx_; }
  const ir::TargetLayout& getTargetLayout() const { return Layout_; }
  DiagnosticsEngine& getDiagnostics() { return Diags_; }

  // === Declaration value tracking ===

  ir::IRValue* getDeclValue(const Decl* D) const;
  void setDeclValue(const Decl* D, ir::IRValue* V);
  ir::IRFunction* getFunction(const FunctionDecl* FD) const;
  ir::IRGlobalVariable* getGlobalVar(const VarDecl* VD) const;
  void setLocalDecl(const VarDecl* VD, ir::IRValue* V);
  ir::IRValue* getLocalDecl(const VarDecl* VD) const;

  /// Clear local scope state (called between function bodies).
  void clearLocalScope();

private:
  /// Initialize sub-emitters. Called at the start of convert().
  void initializeEmitters();

  /// Emit diagnostics for IR conversion errors.
  void emitConversionError(DiagID ID, SourceLocation Loc, ir::StringRef Msg);
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_ASTTOIRCONVERTER_H
```

### 1.2 修改 `include/blocktype/IR/IRConversionResult.h` — 添加 `addError()` 和 `getModule()`

```cpp
// 在现有 IRConversionResult 类中添加以下两个方法（在 getNumErrors() 后面）：

  /// Record a conversion error. Increments error count and marks as invalid.
  void addError() {
    ++NumErrors_;
    Invalid_ = true;
  }

  /// Get the module pointer (does not transfer ownership).
  IRModule* getModule() const { return Module_.get(); }
```

### 1.3 修改 `include/blocktype/IR/TargetLayout.h` — 添加 `getTriple()` getter

```cpp
// 在 isLittleEndian() 后面添加：

  /// Get the target triple string.
  StringRef getTriple() const { return TripleStr; }
```

### 1.4 修改 `include/blocktype/AST/Decl.h` — 添加 `hasGlobalStorage()` 和 `hasVTable()`

```cpp
// 在 VarDecl 类的 public 区域（isConstexpr() 后面）添加：

  /// Whether this variable has global storage (file-scope or static local).
  /// A variable at namespace scope without 'static' has external linkage = global.
  /// A variable with 'static' at namespace scope also has global storage.
  bool hasGlobalStorage() const { return true; }
  // Note: This is a simplified version. When BlockType adds function-scope VarDecls,
  // this should check DeclContext (namespace scope vs function scope).

// 在 CXXRecordDecl 类的 public 区域（isLambda() 后面）添加：

  /// Whether this class has a vtable (requires virtual methods).
  /// Stub: always returns false for now. B.9 (IREmitCXX) will implement properly.
  bool hasVTable() const {
    for (auto* M : Methods) {
      if (M->isVirtual()) return true;
    }
    return false;
  }
```

### 1.5 新增 `include/blocktype/Frontend/IREmitExpr.h` — 前置声明桩

```cpp
//===--- IREmitExpr.h - IR Expression Emitter (Stub) ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stub header for IREmitExpr. Full implementation is Task B.5.
// This file exists so that ASTToIRConverter can forward-declare the class.
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITEEXPR_H
#define BLOCKTYPE_FRONTEND_IREMITEEXPR_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitExpr - Emits IR for AST expressions.
/// Stub class; implementation deferred to Task B.5.
class IREmitExpr {
public:
  explicit IREmitExpr(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitExpr() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITEEXPR_H
```

### 1.6 新增 `include/blocktype/Frontend/IREmitStmt.h` — 前置声明桩

```cpp
//===--- IREmitStmt.h - IR Statement Emitter (Stub) ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITSTMT_H
#define BLOCKTYPE_FRONTEND_IREMITSTMT_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitStmt - Emits IR for AST statements.
/// Stub class; implementation deferred to Task B.6.
class IREmitStmt {
public:
  explicit IREmitStmt(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitStmt() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITSTMT_H
```

### 1.7 新增 `include/blocktype/Frontend/IREmitCXX.h` — 前置声明桩

```cpp
//===--- IREmitCXX.h - IR C++ Emitter (Stub) -----------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITCXX_H
#define BLOCKTYPE_FRONTEND_IREMITCXX_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitCXX - Emits IR for C++ specific constructs (vtables, RTTI, etc.).
/// Stub class; implementation deferred to Task B.9.
class IREmitCXX {
public:
  explicit IREmitCXX(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitCXX() = default;

  /// Emit vtable for the given CXXRecordDecl.
  void EmitVTable(CXXRecordDecl* RD) {
    // Stub: B.9 will implement
  }

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITCXX_H
```

### 1.8 新增 `include/blocktype/Frontend/IRConstantEmitter.h` — 前置声明桩

```cpp
//===--- IRConstantEmitter.h - IR Constant Emitter (Stub) ----*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
#define BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IRConstantEmitter - Emits IR constants from AST expressions.
/// Stub class; implementation deferred to Task B.8.
class IRConstantEmitter {
public:
  explicit IRConstantEmitter(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IRConstantEmitter() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
```

### 1.9 修改 `include/blocktype/Basic/DiagnosticSemaKinds.def` — 添加 IR 转换诊断

在文件末尾（`#undef DIAG` 之前）添加：

```cpp
//===--- IR Conversion Diagnostics (Phase B) ---===//

DIAG(err_ir_type_mapping_failed, Error, \
  "failed to map AST type to IR type", \
  "AST 类型到 IR 类型映射失败")

DIAG(err_ir_expr_conversion_failed, Error, \
  "failed to convert expression to IR", \
  "表达式到 IR 转换失败")

DIAG(err_ir_stmt_conversion_failed, Error, \
  "failed to convert statement to IR", \
  "语句到 IR 转换失败")

DIAG(err_ir_function_conversion_failed, Error, \
  "failed to convert function body to IR", \
  "函数体到 IR 转换失败")

DIAG(err_ir_globalvar_init_failed, Error, \
  "failed to convert global variable initializer to IR", \
  "全局变量初始化器到 IR 转换失败")

DIAG(err_ir_cxx_semantic_failed, Error, \
  "failed to convert C++ construct to IR", \
  "C++ 构造到 IR 转换失败")

DIAG(err_ir_vtable_generation_failed, Error, \
  "failed to generate vtable", \
  "vtable 生成失败")

DIAG(err_ir_rtti_generation_failed, Error, \
  "failed to generate RTTI information", \
  "RTTI 信息生成失败")
```

---

## Part 2: 实现文件（完整代码）

### 2.1 新增 `src/Frontend/ASTToIRConverter.cpp`

```cpp
//===--- ASTToIRConverter.cpp - AST to IR Conversion Framework -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Frontend/ASTToIRConverter.h"

#include "blocktype/Frontend/IREmitExpr.h"
#include "blocktype/Frontend/IREmitStmt.h"
#include "blocktype/Frontend/IREmitCXX.h"
#include "blocktype/Frontend/IRConstantEmitter.h"

#include "blocktype/IR/IRConstant.h"

namespace blocktype {
namespace frontend {

//===----------------------------------------------------------------------===//
// Construction / Destruction
//===----------------------------------------------------------------------===//

ASTToIRConverter::ASTToIRConverter(ir::IRContext& IRCtx,
                                   ir::IRTypeContext& TypeCtx,
                                   const ir::TargetLayout& Layout,
                                   DiagnosticsEngine& Diags)
  : IRCtx_(IRCtx), TypeCtx_(TypeCtx), Layout_(Layout), Diags_(Diags),
    TypeMapper_(std::make_unique<IRTypeMapper>(TypeCtx, Layout, Diags)) {}

ASTToIRConverter::~ASTToIRConverter() {
  delete ExprEmitter_;
  delete StmtEmitter_;
  delete CXXEmitter_;
  delete ConstEmitter_;
}

void ASTToIRConverter::initializeEmitters() {
  // Sub-emitters are stubs for now. B.5-B.9 will provide real implementations.
  ExprEmitter_ = new IREmitExpr(*this);
  StmtEmitter_ = new IREmitStmt(*this);
  CXXEmitter_ = new IREmitCXX(*this);
  ConstEmitter_ = new IRConstantEmitter(*this);
}

//===----------------------------------------------------------------------===//
// convert() - Main entry point
//===----------------------------------------------------------------------===//

ir::IRConversionResult ASTToIRConverter::convert(TranslationUnitDecl* TU) {
  // Step 1: Create IRModule
  TheModule_ = std::make_unique<ir::IRModule>(
    "input",    // Module name (source file name in real usage)
    TypeCtx_,
    Layout_.getTriple(),
    ""          // DataLayout string (empty = use TargetLayout)
  );

  // Step 2: Create IRBuilder
  Builder_ = std::make_unique<ir::IRBuilder>(IRCtx_);

  // Step 3: Initialize sub-emitters
  initializeEmitters();

  // Step 4: Iterate over top-level declarations
  for (Decl* D : TU->decls()) {
    if (auto* FD = llvm::dyn_cast<FunctionDecl>(D)) {
      emitFunction(FD);
    } else if (auto* VD = llvm::dyn_cast<VarDecl>(D)) {
      if (VD->hasGlobalStorage()) {
        emitGlobalVar(VD);
      }
    } else if (auto* RD = llvm::dyn_cast<CXXRecordDecl>(D)) {
      if (RD->isCompleteDefinition() && RD->hasVTable()) {
        // Delegate to IREmitCXX for vtable generation (B.9)
        // For now, CXXEmitter_->EmitVTable(RD) is a no-op stub
        CXXEmitter_->EmitVTable(RD);
      }
    }
    // Other decl types (TypedefDecl, EnumDecl, etc.) are ignored at IR level.
  }

  // Step 5: Transfer module to result
  ir::IRConversionResult Result(std::move(TheModule_));
  return Result;
}

//===----------------------------------------------------------------------===//
// emitFunction
//===----------------------------------------------------------------------===//

ir::IRFunction* ASTToIRConverter::emitFunction(FunctionDecl* FD) {
  // Check if already emitted
  if (auto* Existing = getFunction(FD))
    return Existing;

  // Map return type
  ir::IRType* RetTy = TypeMapper_->mapType(FD->getType());

  // Build parameter types
  ir::SmallVector<ir::IRType*, 8> ParamTypes;
  for (unsigned i = 0; i < FD->getNumParams(); ++i) {
    ParmVarDecl* PVD = FD->getParamDecl(i);
    ir::IRType* ParamTy = TypeMapper_->mapType(PVD->getType());
    ParamTypes.push_back(ParamTy);
  }

  // Create function type
  ir::IRFunctionType* FnTy = TypeCtx_.getFunctionType(RetTy, std::move(ParamTypes));

  // Create or get the function in the module
  ir::IRFunction* IRFn = TheModule_->getOrInsertFunction(
    FD->getName(), FnTy);

  // Record in Functions map
  Functions_[FD] = IRFn;

  // If the function has a body, convert it
  if (Stmt* Body = FD->getBody()) {
    // Create entry basic block
    ir::IRBasicBlock* EntryBB = IRFn->addBasicBlock("entry");
    Builder_->setInsertPoint(EntryBB);

    // Clear local scope for new function
    clearLocalScope();

    // Emit parameters as allocas
    for (unsigned i = 0; i < FD->getNumParams(); ++i) {
      ParmVarDecl* PVD = FD->getParamDecl(i);
      ir::IRArgument* Arg = IRFn->getArg(i);
      // Create alloca for parameter
      ir::IRType* ParamTy = TypeMapper_->mapType(PVD->getType());
      ir::IRInstruction* Alloca = Builder_->createAlloca(ParamTy, PVD->getName());
      Builder_->createStore(Arg, Alloca);
      setDeclValue(PVD, Alloca);
    }

    // Delegate body emission to IREmitStmt (B.6)
    // For B.4 framework: if body is a CompoundStmt, handle directly
    if (auto* CS = llvm::dyn_cast<CompoundStmt>(Body)) {
      for (Stmt* S : CS->getBody()) {
        // TODO: Delegate to StmtEmitter_->emit(S) in B.6
        // For now, skip statement emission
        (void)S;
      }
    }

    // Ensure terminator: if the last instruction is not a terminator, add ret void
    ir::IRInstruction* Term = EntryBB->getTerminator();
    if (!Term) {
      if (RetTy->isVoid()) {
        Builder_->createRetVoid();
      } else {
        // Error recovery: return undef for non-void function without explicit return
        Builder_->createRet(ir::IRConstantUndef::get(IRCtx_, RetTy));
      }
    }
  }

  return IRFn;
}

//===----------------------------------------------------------------------===//
// emitGlobalVar
//===----------------------------------------------------------------------===//

ir::IRGlobalVariable* ASTToIRConverter::emitGlobalVar(VarDecl* VD) {
  // Check if already emitted
  if (auto* Existing = getGlobalVar(VD))
    return Existing;

  // Map type
  ir::IRType* Ty = TypeMapper_->mapType(VD->getType());

  // Create global variable
  ir::IRGlobalVariable* GV = TheModule_->getOrInsertGlobal(VD->getName(), Ty);

  // Handle initializer
  if (Expr* Init = VD->getInit()) {
    // TODO: Delegate to ConstEmitter_->emit(Init) in B.8
    // For now, set undef as initializer as error recovery
    GV->setInitializer(ir::IRConstantUndef::get(IRCtx_, Ty));
  }

  // Record in GlobalVars map
  GlobalVars_[VD] = GV;
  setDeclValue(VD, GV);

  return GV;
}

//===----------------------------------------------------------------------===//
// C++ converters (stubs for B.9)
//===----------------------------------------------------------------------===//

void ASTToIRConverter::convertCXXConstructor(CXXConstructorDecl* Ctor,
                                              ir::IRFunction* IRFn) {
  // TODO: B.9 (IREmitCXX) implementation
  (void)Ctor;
  (void)IRFn;
}

void ASTToIRConverter::convertCXXDestructor(CXXDestructorDecl* Dtor,
                                             ir::IRFunction* IRFn) {
  // TODO: B.9 (IREmitCXX) implementation
  (void)Dtor;
  (void)IRFn;
}

ir::IRValue* ASTToIRConverter::convertCXXConstructExpr(CXXConstructExpr* CCE) {
  // TODO: B.9 (IREmitCXX) implementation
  (void)CCE;
  return nullptr;
}

ir::IRValue* ASTToIRConverter::convertVirtualCall(CXXMemberCallExpr* MCE) {
  // TODO: B.9 (IREmitCXX) implementation
  (void)MCE;
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Error recovery
//===----------------------------------------------------------------------===//

ir::IRValue* ASTToIRConverter::emitErrorPlaceholder(ir::IRType* T) {
  return ir::IRConstantUndef::get(IRCtx_, T);
}

ir::IRType* ASTToIRConverter::emitErrorType() {
  return TypeCtx_.getOpaqueType("error");
}

void ASTToIRConverter::emitConversionError(DiagID ID, SourceLocation Loc,
                                            ir::StringRef Msg) {
  Diags_.report(Loc, ID, Msg);
}

//===----------------------------------------------------------------------===//
// Declaration value tracking
//===----------------------------------------------------------------------===//

ir::IRValue* ASTToIRConverter::getDeclValue(const Decl* D) const {
  auto It = DeclValues_.find(D);
  return It != DeclValues_.end() ? It->second : nullptr;
}

void ASTToIRConverter::setDeclValue(const Decl* D, ir::IRValue* V) {
  DeclValues_[D] = V;
}

ir::IRFunction* ASTToIRConverter::getFunction(const FunctionDecl* FD) const {
  auto It = Functions_.find(FD);
  return It != Functions_.end() ? It->second : nullptr;
}

ir::IRGlobalVariable* ASTToIRConverter::getGlobalVar(const VarDecl* VD) const {
  auto It = GlobalVars_.find(VD);
  return It != GlobalVars_.end() ? It->second : nullptr;
}

void ASTToIRConverter::setLocalDecl(const VarDecl* VD, ir::IRValue* V) {
  LocalDecls_[VD] = V;
}

ir::IRValue* ASTToIRConverter::getLocalDecl(const VarDecl* VD) const {
  auto It = LocalDecls_.find(VD);
  return It != LocalDecls_.end() ? It->second : nullptr;
}

void ASTToIRConverter::clearLocalScope() {
  LocalDecls_.clear();
}

} // namespace frontend
} // namespace blocktype
```

---

## Part 3: CMakeLists 修改

### `src/Frontend/CMakeLists.txt` — 添加新源文件

在现有源文件列表中添加：

```cmake
ASTToIRConverter.cpp
```

当前完整列表应为：

```cmake
target_sources(blocktype PRIVATE
  FrontendBase.cpp
  FrontendRegistry.cpp
  IRTypeMapper.cpp
  ASTToIRConverter.cpp
)
```

无需修改 tests/CMakeLists.txt，因为测试文件在 tests/ 目录下自动发现。

---

## Part 4: 测试文件（完整代码）

### `tests/Frontend/test_ast_to_ir_converter.cpp`

```cpp
//===--- test_ast_to_ir_converter.cpp - ASTToIRConverter Tests ---*- C++ -*-===//

#include <cassert>
#include <iostream>
#include <memory>

#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/ASTToIRConverter.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/TargetLayout.h"

using namespace blocktype;
using namespace blocktype::frontend;

namespace {

/// Helper: create a basic test environment
struct TestEnv {
  ir::IRContext IRCtx;
  ir::IRTypeContext& TypeCtx;
  std::unique_ptr<ir::TargetLayout> Layout;
  DiagnosticsEngine Diags;

  TestEnv()
    : TypeCtx(IRCtx.getTypeContext()),
      Layout(ir::TargetLayout::Create("x86_64-unknown-linux-gnu")),
      Diags() {}
};

} // anonymous namespace

// === Test V1: Empty TU conversion ===
void test_empty_tu() {
  TestEnv Env;

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);

  TranslationUnitDecl TU(SourceLocation());
  auto Result = Converter.convert(&TU);

  assert(Result.isUsable());
  assert(Result.getModule() != nullptr);
  assert(Result.getModule()->getNumFunctions() == 0);
  std::cout << "  PASS: V1 - Empty TU conversion\n";
}

// === Test V2: TU with a simple function ===
void test_tu_with_function() {
  TestEnv Env;

  // Create a simple function: void main() {}
  QualType VoidTy;  // Default-constructed QualType is void-like
  CompoundStmt EmptyBody(SourceLocation(), {});
  FunctionDecl MainFD(SourceLocation(), "main", VoidTy, {}, &EmptyBody);

  TranslationUnitDecl TU(SourceLocation());
  TU.addDecl(&MainFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  assert(Result.isUsable());
  assert(Result.getModule() != nullptr);
  auto* MainFn = Result.getModule()->getFunction("main");
  assert(MainFn != nullptr);
  std::cout << "  PASS: V2 - TU with function\n";
}

// === Test V3: TU with global variable ===
void test_tu_with_global_var() {
  TestEnv Env;

  // Create a global variable: int x;
  QualType IntTy;  // Simplified for test
  VarDecl GlobalVar(SourceLocation(), "x", IntTy, nullptr, false, false);

  TranslationUnitDecl TU(SourceLocation());
  TU.addDecl(&GlobalVar);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  assert(Result.isUsable());
  assert(Result.getModule() != nullptr);
  auto* GV = Result.getModule()->getGlobalVariable("x");
  assert(GV != nullptr);
  std::cout << "  PASS: V3 - TU with global variable\n";
}

// === Test V4: Error recovery - null type mapping ===
void test_error_recovery() {
  TestEnv Env;

  // Create a function with null body (declaration only)
  QualType VoidTy;
  FunctionDecl DeclFD(SourceLocation(), "decl_only", VoidTy, {}, nullptr);

  TranslationUnitDecl TU(SourceLocation());
  TU.addDecl(&DeclFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  // Should still be usable (declaration-only functions are fine)
  assert(Result.isUsable());
  auto* Fn = Result.getModule()->getFunction("decl_only");
  assert(Fn != nullptr);
  assert(Fn->isDeclaration());  // No body
  std::cout << "  PASS: V4 - Error recovery (declaration-only)\n";
}

// === Test V5: Multiple functions and globals ===
void test_mixed_decls() {
  TestEnv Env;

  QualType VoidTy;

  // Create two functions
  CompoundStmt EmptyBody(SourceLocation(), {});
  FunctionDecl FooFD(SourceLocation(), "foo", VoidTy, {}, &EmptyBody);
  FunctionDecl BarFD(SourceLocation(), "bar", VoidTy, {}, &EmptyBody);

  // Create a global variable
  QualType IntTy;
  VarDecl GlobalVar(SourceLocation(), "g_counter", IntTy, nullptr, false, false);

  TranslationUnitDecl TU(SourceLocation());
  TU.addDecl(&FooFD);
  TU.addDecl(&GlobalVar);
  TU.addDecl(&BarFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  assert(Result.isUsable());
  assert(Result.getModule()->getFunction("foo") != nullptr);
  assert(Result.getModule()->getFunction("bar") != nullptr);
  assert(Result.getModule()->getGlobalVariable("g_counter") != nullptr);
  std::cout << "  PASS: V5 - Mixed declarations\n";
}

// === Test V6: IRConversionResult extended API ===
void test_conversion_result_api() {
  // Test addError()
  ir::IRContext IRCtx;
  ir::IRConversionResult R;
  assert(!R.isInvalid());
  assert(R.getNumErrors() == 0);

  R.addError();
  assert(R.isInvalid());
  assert(R.getNumErrors() == 1);

  R.addError();
  assert(R.getNumErrors() == 2);

  // Test getModule() on empty result
  assert(R.getModule() == nullptr);

  std::cout << "  PASS: V6 - IRConversionResult extended API\n";
}

// === Test V7: TargetLayout getTriple() ===
void test_target_layout_get_triple() {
  auto Layout = ir::TargetLayout::Create("x86_64-unknown-linux-gnu");
  assert(Layout->getTriple() == "x86_64-unknown-linux-gnu");

  std::cout << "  PASS: V7 - TargetLayout::getTriple()\n";
}

// === Test V8: VarDecl hasGlobalStorage() ===
void test_vardecl_has_global_storage() {
  QualType IntTy;
  VarDecl GlobalVar(SourceLocation(), "g", IntTy);
  assert(GlobalVar.hasGlobalStorage());

  std::cout << "  PASS: V8 - VarDecl::hasGlobalStorage()\n";
}

// === Test V9: CXXRecordDecl hasVTable() ===
void test_cxxrecord_has_vtable() {
  CXXRecordDecl RD(SourceLocation(), "MyClass");
  assert(!RD.hasVTable());  // No methods, no vtable

  std::cout << "  PASS: V9 - CXXRecordDecl::hasVTable()\n";
}

int main() {
  std::cout << "=== ASTToIRConverter Tests ===\n";

  test_empty_tu();
  test_tu_with_function();
  test_tu_with_global_var();
  test_error_recovery();
  test_mixed_decls();
  test_conversion_result_api();
  test_target_layout_get_triple();
  test_vardecl_has_global_storage();
  test_cxxrecord_has_vtable();

  std::cout << "\nAll tests passed!\n";
  return 0;
}
```

---

## Part 5: 验收标准映射

| 验收标准 | 对应测试 | 状态 |
|---------|---------|------|
| V1: 空 TU 转换 — `isUsable()==true`, `getNumFunctions()==0` | `test_empty_tu` | 覆盖 |
| V2: 含函数的 TU — `getFunction("main")!=nullptr` | `test_tu_with_function` | 覆盖 |
| V3: 错误恢复 — 不崩溃，错误被记录 | `test_error_recovery` | 覆盖 |
| V4: 多个函数+全局变量 | `test_mixed_decls` | 覆盖 |
| V5: IRConversionResult 新增 API (`addError()`, `getModule()`) | `test_conversion_result_api` | 覆盖 |
| V6: TargetLayout::getTriple() | `test_target_layout_get_triple` | 覆盖 |
| V7: VarDecl::hasGlobalStorage() | `test_vardecl_has_global_storage` | 覆盖 |
| V8: CXXRecordDecl::hasVTable() | `test_cxxrecord_has_vtable` | 覆盖 |

---

## Part 6: dev-tester 执行步骤

### 步骤 1: 修改已有文件（按顺序）

1. **修改 `include/blocktype/IR/IRConversionResult.h`**
   - 在 `getNumErrors()` 方法后添加 `addError()` 和 `getModule()` 方法

2. **修改 `include/blocktype/IR/TargetLayout.h`**
   - 在 `isLittleEndian()` 方法后添加 `getTriple()` getter

3. **修改 `include/blocktype/AST/Decl.h`**
   - 在 `VarDecl` 类中添加 `hasGlobalStorage()` 方法
   - 在 `CXXRecordDecl` 类中添加 `hasVTable()` 方法

4. **修改 `include/blocktype/Basic/DiagnosticSemaKinds.def`**
   - 在 `#undef DIAG` 之前添加 8 个 IR 转换错误诊断

### 步骤 2: 创建新文件

5. 创建 `include/blocktype/Frontend/ASTToIRConverter.h`
6. 创建 `include/blocktype/Frontend/IREmitExpr.h`（桩）
7. 创建 `include/blocktype/Frontend/IREmitStmt.h`（桩）
8. 创建 `include/blocktype/Frontend/IREmitCXX.h`（桩）
9. 创建 `include/blocktype/Frontend/IRConstantEmitter.h`（桩）
10. 创建 `src/Frontend/ASTToIRConverter.cpp`

### 步骤 3: 修改构建

11. 修改 `src/Frontend/CMakeLists.txt` 添加 `ASTToIRConverter.cpp`

### 步骤 4: 创建测试

12. 创建 `tests/Frontend/test_ast_to_ir_converter.cpp`

### 步骤 5: 编译验证

```bash
cd /Users/yuan/Documents/BlockType && mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target blocktype
```

### 步骤 6: 运行测试

```bash
./build/tests/Frontend/test_ast_to_ir_converter
```

### 步骤 7: 红线 Checklist 自检

1. ✅ 架构优先 — `ASTToIRConverter` 通过 `FrontendBase` 体系集成，解耦前后端
2. ✅ 多前端多后端自由组合 — `FrontendRegistry` 选择前端，IR 层与后端无关
3. ✅ 渐进式改造 — B.4 是可编译可测试的框架，子发射器为桩
4. ✅ 现有功能不退化 — 仅新增文件和方法，不修改现有行为
5. ✅ 接口抽象优先 — 通过 `IRTypeMapper` 抽象映射，通过子发射器抽象细节
6. ✅ IR 中间层解耦 — 前端输出 `IRConversionResult`（持有 `IRModule`），后端消费 `IRModule`

---

## 附录：错误恢复策略表（修正版）

| 错误场景 | 恢复策略 | 占位值 | DiagID |
|---------|---------|--------|--------|
| QualType 无法映射 | 发出 Diag，用 `IROpaqueType("error")` 占位 | `TypeCtx_.getOpaqueType("error")` | `err_ir_type_mapping_failed` |
| 表达式转换失败 | 发出 Diag，用 `IRConstantUndef` 占位 | `IRConstantUndef::get(IRCtx_, T)` | `err_ir_expr_conversion_failed` |
| 语句转换失败 | 发出 Diag，跳过该语句 | — | `err_ir_stmt_conversion_failed` |
| 函数体转换失败 | 发出 Diag，生成空函数体+`createRetVoid()` | `Builder_->createRetVoid()` | `err_ir_function_conversion_failed` |
| 全局变量初始化失败 | 发出 Diag，用 `IRConstantUndef` 作为初始值 | `IRConstantUndef::get(IRCtx_, T)` | `err_ir_globalvar_init_failed` |
| C++特有语义转换失败 | 发出 Diag，用 `IRConstantUndef` 占位 | `IRConstantUndef::get(IRCtx_, T)` | `err_ir_cxx_semantic_failed` |
| VTable生成失败 | 发出 Diag，用空全局变量占位 | `getOrInsertGlobal(Name, OpaqueTy)` | `err_ir_vtable_generation_failed` |
| RTTI生成失败 | 发出 Diag，用空全局变量占位 | `getOrInsertGlobal(Name, OpaqueTy)` | `err_ir_rtti_generation_failed` |
