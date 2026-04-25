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

  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {});
  auto* F = M->getOrInsertFunction("no_fold_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createRet(Builder.getInt32(42));

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
