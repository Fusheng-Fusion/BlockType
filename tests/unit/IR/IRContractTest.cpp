#include <gtest/gtest.h>

#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRContract.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRConversionResult.h"
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
