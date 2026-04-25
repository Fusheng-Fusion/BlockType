# Task A-F11：IR 优化 Pass 具体实现 + 前端-IR 契约测试 — 优化版

## 红线 Checklist 确认

| # | 红线 | 状态 | 说明 |
|---|------|------|------|
| 1 | 架构优先 | ✅ | Pass/PassManager 遵循已有 Pass 抽象基类，IRContract 独立于 VerifierPass |
| 2 | 多前端多后端自由组合 | ✅ | IR 层 Pass 不依赖任何前端/后端具体实现 |
| 3 | 渐进式改造 | ✅ | 新增 6 个文件 + 修改 2 个 CMakeLists，不影响已有功能 |
| 4 | 现有功能不退化 | ✅ | 纯新增，不修改任何已有头文件或源文件 |
| 5 | 接口抽象优先 | ✅ | PassManager 通过 Pass 抽象接口管理所有 Pass |
| 6 | IR 中间层解耦 | ✅ | IRContract 验证 IRModule 的结构契约，与前端/后端无关 |

## 依赖
- A.5（IRBuilder）✅、A.6（IRVerifier）✅、A-F9（IRVerificationResult）✅

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRPasses.h` |
| 新增 | `src/IR/IRPasses.cpp` |
| 新增 | `include/blocktype/IR/IRContract.h` |
| 新增 | `src/IR/IRContract.cpp` |
| 新增 | `tests/unit/IR/IRPassesTest.cpp` |
| 新增 | `tests/unit/IR/IRContractTest.cpp` |
| 修改 | `src/IR/CMakeLists.txt`（+2 行） |
| 修改 | `tests/unit/IR/CMakeLists.txt`（+2 行） |

## 设计要点

1. **PassManager**：`SmallVector<unique_ptr<Pass>, 8>`，`addPass<T>(args...)` + `runAll(M)` → bool
2. **DeadFunctionEliminationPass**：移除 `isDeclaration() && Linkage==Internal` 的函数，erase-remove 惯用法
3. **ConstantFoldingPass**：遍历指令，对 Add/Sub/Mul 且双操作数为 `IRConstantInt` 的指令做折叠，`replaceAllUsesWith` + `erase`
4. **TypeCanonicalizationPass**：轻量级占位实现，返回 false
5. **IRContract**：7 个独立契约函数 + `verifyAllContracts` 聚合返回 `IRVerificationResult`

---

## Part 1：头文件

### 1.1 `include/blocktype/IR/IRPasses.h`

```cpp
#ifndef BLOCKTYPE_IR_IRPASSES_H
#define BLOCKTYPE_IR_IRPASSES_H

#include <memory>

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRPass.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRConstantInt;

// ============================================================================
// PassManager — 管理并顺序执行 IR Pass
// ============================================================================

class PassManager {
  SmallVector<std::unique_ptr<Pass>, 8> Passes_;

public:
  PassManager() = default;

  /// 添加一个 Pass 到管线末尾。
  template <typename PassT, typename... Args>
  void addPass(Args&&... args) {
    Passes_.push_back(std::make_unique<PassT>(std::forward<Args>(args)...));
  }

  /// 顺序执行所有 Pass。返回 true 如果任意 Pass 返回 true（即修改了 IR）。
  bool runAll(IRModule& M);

  /// 返回管线中的 Pass 数量。
  size_t size() const { return Passes_.size(); }
};

// ============================================================================
// DeadFunctionEliminationPass — 移除无用的内部声明函数
// ============================================================================

class DeadFunctionEliminationPass : public Pass {
public:
  StringRef getName() const override { return "dead-func-elim"; }

  /// 移除所有 isDeclaration() && Linkage == Internal 的函数。
  /// 返回 true 如果有函数被移除。
  bool run(IRModule& M) override;
};

// ============================================================================
// ConstantFoldingPass — 常量折叠
// ============================================================================

class ConstantFoldingPass : public Pass {
public:
  StringRef getName() const override { return "constant-fold"; }

  /// 对所有整数算术指令做常量折叠。
  /// 返回 true 如果有指令被折叠。
  bool run(IRModule& M) override;

private:
  /// 尝试折叠一条指令。返回折叠后的常量，或 std::nullopt。
  std::optional<IRConstantInt*> tryFold(class IRInstruction& I);
};

// ============================================================================
// TypeCanonicalizationPass — 类型规范化
// ============================================================================

class TypeCanonicalizationPass : public Pass {
public:
  StringRef getName() const override { return "type-canon"; }

  /// 轻量级类型规范化检查。当前阶段为占位实现。
  bool run(IRModule& M) override;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRPASSES_H
```

### 1.2 `include/blocktype/IR/IRContract.h`

```cpp
#ifndef BLOCKTYPE_IR_IRCONTRACT_H
#define BLOCKTYPE_IR_IRCONTRACT_H

#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRVerificationResult;

namespace contract {

/// 验证模块级契约：名称非空。
bool verifyIRModuleContract(const IRModule& M);

/// 验证所有类型完整性：无 OpaqueType 出现在函数签名或全局变量中。
bool verifyTypeCompleteness(const IRModule& M);

/// 验证函数非空：所有定义函数至少有一个 BasicBlock。
bool verifyFunctionNonEmpty(const IRModule& M);

/// 验证终结指令契约：每个 BasicBlock 都有终结指令。
bool verifyTerminatorContract(const IRModule& M);

/// 验证类型一致性：指令操作数类型与指令语义匹配。
bool verifyTypeConsistency(const IRModule& M);

/// 验证 TargetTriple 格式：如果设置了 TargetTriple，应包含至少一个 '-'。
bool verifyTargetTripleValid(const IRModule& M);

/// 执行所有契约验证，返回聚合结果。
IRVerificationResult verifyAllContracts(const IRModule& M);

} // namespace contract
} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCONTRACT_H
```

---

## Part 2：实现文件

### 2.1 `src/IR/IRPasses.cpp`

```cpp
#include "blocktype/IR/IRPasses.h"

#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRValue.h"

#include <algorithm>

namespace blocktype {
namespace ir {

// ============================================================================
// PassManager
// ============================================================================

bool PassManager::runAll(IRModule& M) {
  bool AnyChanged = false;
  for (auto& P : Passes_) {
    if (P->run(M)) {
      AnyChanged = true;
    }
  }
  return AnyChanged;
}

// ============================================================================
// DeadFunctionEliminationPass
// ============================================================================

bool DeadFunctionEliminationPass::run(IRModule& M) {
  auto& Functions = M.getFunctions();
  size_t OriginalSize = Functions.size();

  Functions.erase(
    std::remove_if(Functions.begin(), Functions.end(),
      [](const std::unique_ptr<IRFunction>& F) {
        return F->isDeclaration() &&
               F->getLinkage() == LinkageKind::Internal;
      }),
    Functions.end());

  return Functions.size() < OriginalSize;
}

// ============================================================================
// ConstantFoldingPass
// ============================================================================

bool ConstantFoldingPass::run(IRModule& M) {
  bool Changed = false;

  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;

    for (auto& BB : Func->getBasicBlocks()) {
      SmallVector<IRInstruction*, 16> ToFold;
      for (auto& Inst : BB->getInstList()) {
        if (auto Folded = tryFold(*Inst)) {
          (void)Folded;
          ToFold.push_back(Inst.get());
        }
      }

      for (auto* Inst : ToFold) {
        auto Folded = tryFold(*Inst);
        if (Folded) {
          Inst->replaceAllUsesWith(*Folded);
          BB->erase(Inst);
          Changed = true;
        }
      }
    }
  }

  return Changed;
}

std::optional<IRConstantInt*> ConstantFoldingPass::tryFold(IRInstruction& I) {
  Opcode Op = I.getOpcode();

  if (Op != Opcode::Add && Op != Opcode::Sub && Op != Opcode::Mul)
    return std::nullopt;

  if (I.getNumOperands() != 2)
    return std::nullopt;

  auto* LHS = I.getOperand(0);
  auto* RHS = I.getOperand(1);

  if (!IRConstantInt::classof(LHS) || !IRConstantInt::classof(RHS))
    return std::nullopt;

  auto* LC = static_cast<IRConstantInt*>(LHS);
  auto* RC = static_cast<IRConstantInt*>(RHS);

  unsigned BW = static_cast<IRIntegerType*>(LC->getType())->getBitWidth();
  if (static_cast<IRIntegerType*>(RC->getType())->getBitWidth() != BW)
    return std::nullopt;

  uint64_t Result = 0;
  switch (Op) {
  case Opcode::Add:
    Result = LC->getZExtValue() + RC->getZExtValue();
    break;
  case Opcode::Sub:
    Result = LC->getZExtValue() - RC->getZExtValue();
    break;
  case Opcode::Mul:
    Result = LC->getZExtValue() * RC->getZExtValue();
    break;
  default:
    return std::nullopt;
  }

  auto* FoldedConst = new IRConstantInt(static_cast<IRIntegerType*>(LC->getType()), Result);
  return FoldedConst;
}

// ============================================================================
// TypeCanonicalizationPass
// ============================================================================

bool TypeCanonicalizationPass::run(IRModule& M) {
  (void)M;
  return false;
}

} // namespace ir
} // namespace blocktype
```

### 2.2 `src/IR/IRContract.cpp`

```cpp
#include "blocktype/IR/IRContract.h"

#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRType.h"
#include "blocktype/IR/IRValue.h"

namespace blocktype {
namespace ir {
namespace contract {

bool verifyIRModuleContract(const IRModule& M) {
  if (M.getName().empty())
    return false;
  return true;
}

bool verifyTypeCompleteness(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    auto* FTy = Func->getFunctionType();
    if (FTy->getReturnType()->isOpaque())
      return false;
    for (unsigned i = 0; i < FTy->getNumParams(); ++i) {
      if (FTy->getParamType(i)->isOpaque())
        return false;
    }
  }
  for (auto& GV : M.getGlobals()) {
    if (GV->getType()->isOpaque())
      return false;
  }
  return true;
}

bool verifyFunctionNonEmpty(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    if (Func->getNumBasicBlocks() == 0)
      return false;
  }
  return true;
}

bool verifyTerminatorContract(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    for (auto& BB : Func->getBasicBlocks()) {
      if (!BB->getTerminator())
        return false;
    }
  }
  return true;
}

bool verifyTypeConsistency(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    for (auto& BB : Func->getBasicBlocks()) {
      for (auto& Inst : BB->getInstList()) {
        if (Inst->isBinaryOp()) {
          if (Inst->getNumOperands() >= 2) {
            auto* LTy = Inst->getOperand(0)->getType();
            auto* RTy = Inst->getOperand(1)->getType();
            if (LTy && RTy && !LTy->equals(RTy))
              return false;
          }
        }
      }
    }
  }
  return true;
}

bool verifyTargetTripleValid(const IRModule& M) {
  StringRef Triple = M.getTargetTriple();
  if (Triple.empty())
    return true;
  if (Triple.find('-') == StringRef::npos)
    return false;
  return true;
}

IRVerificationResult verifyAllContracts(const IRModule& M) {
  IRVerificationResult Result(true);

  if (!verifyIRModuleContract(M))
    Result.addViolation("IRModule contract violated: empty module name");

  if (!verifyTypeCompleteness(M))
    Result.addViolation("Type completeness violated: opaque types in function signatures or globals");

  if (!verifyFunctionNonEmpty(M))
    Result.addViolation("Function non-empty violated: defined function has no basic blocks");

  if (!verifyTerminatorContract(M))
    Result.addViolation("Terminator contract violated: basic block missing terminator");

  if (!verifyTypeConsistency(M))
    Result.addViolation("Type consistency violated: operand type mismatch in binary operation");

  if (!verifyTargetTripleValid(M))
    Result.addViolation("Target triple invalid: malformed triple string");

  return Result;
}

} // namespace contract
} // namespace ir
} // namespace blocktype
```

---

## Part 3：CMakeLists 修改

### 3.1 `src/IR/CMakeLists.txt` — 在 `IRPass.cpp` 后添加 2 行

在 `IRPass.cpp` 之后添加：
```
  IRPasses.cpp
  IRContract.cpp
```

### 3.2 `tests/unit/IR/CMakeLists.txt` — 在 `IRErrorCodeTest.cpp` 后添加 2 行

在 `IRErrorCodeTest.cpp` 之后添加：
```
  IRPassesTest.cpp
  IRContractTest.cpp
```

---

## Part 4：测试文件

### 4.1 `tests/unit/IR/IRPassesTest.cpp`

```cpp
#include <gtest/gtest.h>

#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRPasses.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/IRValue.h"

using namespace blocktype;
using namespace blocktype::ir;

static std::unique_ptr<IRModule> buildLegalModule(IRContext& Ctx) {
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("test_module", TCtx, "x86_64-unknown-linux-gnu");

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty(), TCtx.getInt32Ty()});
  auto* F = M->getOrInsertFunction("add", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  auto* V1 = Builder.getInt32(10);
  auto* V2 = Builder.getInt32(20);
  auto* Sum = Builder.createAdd(V1, V2, "sum");
  Builder.createRet(Sum);

  return M;
}

// V1: DFE 移除内部声明函数
TEST(IRPassesTest, DeadFunctionEliminationRemovesInternalDecls) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("test", TCtx, "x86_64-unknown-linux-gnu");

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty()});
  auto* DefFunc = M->getOrInsertFunction("defined_func", FTy);
  auto* Entry = DefFunc->addBasicBlock("entry");
  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createRet(Builder.getInt32(42));

  size_t BeforeCount = M->getNumFunctions();

  auto* DeadFTy = TCtx.getFunctionType(TCtx.getVoidType(), {});
  auto DeadF = std::make_unique<IRFunction>(nullptr, "internal_dead", DeadFTy,
                                             LinkageKind::Internal);
  M->addFunction(std::move(DeadF));
  EXPECT_EQ(M->getNumFunctions(), BeforeCount + 1);

  DeadFunctionEliminationPass DFE;
  bool Changed = DFE.run(*M);

  EXPECT_TRUE(Changed);
  EXPECT_EQ(M->getNumFunctions(), BeforeCount);
  EXPECT_NE(M->getFunction("defined_func"), nullptr);
  EXPECT_EQ(M->getFunction("internal_dead"), nullptr);
}

// V1b: DFE 不移除 External 声明
TEST(IRPassesTest, DeadFunctionEliminationKeepsExternalDecls) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("test", TCtx);

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {});
  M->getOrInsertFunction("extern_decl", FTy);
  size_t BeforeCount = M->getNumFunctions();

  DeadFunctionEliminationPass DFE;
  bool Changed = DFE.run(*M);

  EXPECT_FALSE(Changed);
  EXPECT_EQ(M->getNumFunctions(), BeforeCount);
}

// V2: CF 折叠常量加法
TEST(IRPassesTest, ConstantFoldingFoldsAdd) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);

  ConstantFoldingPass CF;
  bool Changed = CF.run(*M);
  EXPECT_TRUE(Changed);
}

// V2b: CF 折叠乘法
TEST(IRPassesTest, ConstantFoldingFoldsMul) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("fold_test", TCtx, "x86_64-unknown-linux-gnu");

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {});
  auto* F = M->getOrInsertFunction("mul_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  auto* V1 = Builder.getInt32(3);
  auto* V2 = Builder.getInt32(7);
  auto* Prod = Builder.createMul(V1, V2, "prod");
  Builder.createRet(Prod);

  ConstantFoldingPass CF;
  bool Changed = CF.run(*M);
  EXPECT_TRUE(Changed);
}

// V2c: CF 无常量可折叠
TEST(IRPassesTest, ConstantFoldingNoChange) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("no_fold", TCtx, "x86_64-unknown-linux-gnu");

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty()});
  auto* F = M->getOrInsertFunction("no_fold_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  auto* Arg = F->getArg(0);
  auto* Result = Builder.createAdd(Arg, Arg, "result");
  Builder.createRet(Result);

  ConstantFoldingPass CF;
  bool Changed = CF.run(*M);
  EXPECT_FALSE(Changed);
}

// V3: TypeCanonicalizationPass 占位返回 false
TEST(IRPassesTest, TypeCanonicalizationNoChange) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);

  TypeCanonicalizationPass TC;
  bool Changed = TC.run(*M);
  EXPECT_FALSE(Changed);
}

// PassManager 管线执行
TEST(IRPassesTest, PassManagerRunsAll) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);

  PassManager PM;
  PM.addPass<ConstantFoldingPass>();
  PM.addPass<TypeCanonicalizationPass>();
  EXPECT_EQ(PM.size(), 2u);

  bool Changed = PM.runAll(*M);
  EXPECT_TRUE(Changed);
}

// PassManager 空
TEST(IRPassesTest, PassManagerEmpty) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);

  PassManager PM;
  EXPECT_EQ(PM.size(), 0u);
  bool Changed = PM.runAll(*M);
  EXPECT_FALSE(Changed);
}

// Pass getName
TEST(IRPassesTest, GetName) {
  DeadFunctionEliminationPass DFE;
  ConstantFoldingPass CF;
  TypeCanonicalizationPass TC;
  EXPECT_EQ(DFE.getName(), "dead-func-elim");
  EXPECT_EQ(CF.getName(), "constant-fold");
  EXPECT_EQ(TC.getName(), "type-canon");
}
```

### 4.2 `tests/unit/IR/IRContractTest.cpp`

```cpp
#include <gtest/gtest.h>

#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRContract.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/IRValue.h"

using namespace blocktype;
using namespace blocktype::ir;
using namespace blocktype::ir::contract;

static std::unique_ptr<IRModule> buildLegalModule(IRContext& Ctx) {
  IRTypeContext& TCtx = Ctx.getTypeContext();
  auto M = std::make_unique<IRModule>("legal_module", TCtx, "x86_64-unknown-linux-gnu");

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty(), TCtx.getInt32Ty()});
  auto* F = M->getOrInsertFunction("add", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  auto* V1 = Builder.getInt32(10);
  auto* V2 = Builder.getInt32(20);
  auto* Sum = Builder.createAdd(V1, V2, "sum");
  Builder.createRet(Sum);

  return M;
}

// V4: 合法模块通过全部契约
TEST(IRContractTest, AllContractsPassForLegalModule) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  auto Result = verifyAllContracts(*M);
  EXPECT_TRUE(Result.isValid());
  EXPECT_EQ(Result.getNumViolations(), 0u);
}

// V5: 各个单独契约
TEST(IRContractTest, VerifyIRModuleContract) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyIRModuleContract(*M));
}

TEST(IRContractTest, VerifyIRModuleContractEmptyName) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("", TCtx, "x86_64-unknown-linux-gnu");
  EXPECT_FALSE(verifyIRModuleContract(M));
}

TEST(IRContractTest, VerifyTypeCompletenessLegal) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyTypeCompleteness(*M));
}

TEST(IRContractTest, VerifyTypeCompletenessWithOpaque) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("opaque_test", TCtx);

  auto* Opaque = TCtx.getOpaqueType("unresolved");
  auto* FTy = TCtx.getFunctionType(Opaque, {});
  M.getOrInsertFunction("bad_func", FTy);

  EXPECT_FALSE(verifyTypeCompleteness(M));
}

TEST(IRContractTest, VerifyFunctionNonEmptyLegal) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyFunctionNonEmpty(*M));
}

TEST(IRContractTest, VerifyFunctionNonEmptyDeclarationOnly) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("decl_only", TCtx);

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {});
  M.getOrInsertFunction("extern_fn", FTy);
  EXPECT_TRUE(verifyFunctionNonEmpty(M));
}

TEST(IRContractTest, VerifyTerminatorContractLegal) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyTerminatorContract(*M));
}

TEST(IRContractTest, VerifyTerminatorContractMissing) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("no_term", TCtx);

  auto* FTy = TCtx.getFunctionType(TCtx.getVoidType(), {});
  auto* F = M.getOrInsertFunction("no_term_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createAdd(Builder.getInt32(1), Builder.getInt32(2), "dead");

  EXPECT_FALSE(verifyTerminatorContract(M));
}

TEST(IRContractTest, VerifyTypeConsistencyLegal) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyTypeConsistency(*M));
}

TEST(IRContractTest, VerifyTargetTripleValid) {
  IRContext Ctx;
  auto M = buildLegalModule(Ctx);
  EXPECT_TRUE(verifyTargetTripleValid(*M));
}

TEST(IRContractTest, VerifyTargetTripleEmpty) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("no_triple", TCtx);
  EXPECT_TRUE(verifyTargetTripleValid(M));
}

TEST(IRContractTest, VerifyTargetTripleMalformed) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("bad_triple", TCtx);
  M.setTargetTriple("invalidtriple");
  EXPECT_FALSE(verifyTargetTripleValid(M));
}

// 多重违规聚合
TEST(IRContractTest, VerifyAllContractsMultipleViolations) {
  IRContext Ctx;
  IRTypeContext& TCtx = Ctx.getTypeContext();
  IRModule M("", TCtx, "badtriple");

  auto Result = verifyAllContracts(M);
  EXPECT_FALSE(Result.isValid());
  EXPECT_GE(Result.getNumViolations(), 2u);
}
```

---

## Part 5：验收标准映射

| 验收标准 | 测试用例 |
|----------|----------|
| V1: DFE 移除内部声明 | `IRPassesTest.DeadFunctionEliminationRemovesInternalDecls` |
| V1b: DFE 保留 External | `IRPassesTest.DeadFunctionEliminationKeepsExternalDecls` |
| V2: CF 折叠加法 | `IRPassesTest.ConstantFoldingFoldsAdd` |
| V2b: CF 折叠乘法 | `IRPassesTest.ConstantFoldingFoldsMul` |
| V2c: CF 无变化 | `IRPassesTest.ConstantFoldingNoChange` |
| V3: TC 占位 | `IRPassesTest.TypeCanonicalizationNoChange` |
| V4: 全契约通过 | `IRContractTest.AllContractsPassForLegalModule` |
| V5: 各单独契约 | `IRContractTest.VerifyIRModuleContract` 等 |

---

## Part 6：dev-tester 执行步骤

1. 创建 `include/blocktype/IR/IRPasses.h`（复制 Part 1.1）
2. 创建 `src/IR/IRPasses.cpp`（复制 Part 2.1）
3. 创建 `include/blocktype/IR/IRContract.h`（复制 Part 1.2）
4. 创建 `src/IR/IRContract.cpp`（复制 Part 2.2）
5. 创建 `tests/unit/IR/IRPassesTest.cpp`（复制 Part 4.1）
6. 创建 `tests/unit/IR/IRContractTest.cpp`（复制 Part 4.2）
7. 修改 `src/IR/CMakeLists.txt`：在 `IRPass.cpp` 后添加 `IRPasses.cpp` 和 `IRContract.cpp`
8. 修改 `tests/unit/IR/CMakeLists.txt`：在 `IRErrorCodeTest.cpp` 后添加 `IRPassesTest.cpp` 和 `IRContractTest.cpp`
9. 编译并运行测试：`cd build && cmake --build . --target blocktype-ir-test && ./test/unit/IR/blocktype-ir-test`
10. 确认所有测试通过
