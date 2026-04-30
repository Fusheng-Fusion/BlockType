#include <gtest/gtest.h>
#include <memory>
#include "blocktype/IR/IREquivalenceChecker.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRTypeContext.h"

using namespace blocktype::ir;

namespace {

/// 辅助：创建包含一个空函数的模块
std::unique_ptr<IRModule> makeModuleWithFunc(
    IRTypeContext& Ctx, StringRef ModName, StringRef FuncName) {
  auto Mod = std::make_unique<IRModule>(ModName, Ctx);
  auto* VoidTy = Ctx.getVoidType();
  auto* FnTy = Ctx.getFunctionType(VoidTy, SmallVector<IRType*, 8>{});
  auto Fn = std::make_unique<IRFunction>(Mod.get(), FuncName, FnTy);
  Mod->addFunction(std::move(Fn));
  return Mod;
}

} // anonymous namespace

TEST(IREquivalenceCheckerTest, SameModuleIsEquivalent) {
  IRTypeContext Ctx;
  auto Mod = makeModuleWithFunc(Ctx, "test", "main");
  auto Result = IREquivalenceChecker::check(*Mod, *Mod);
  EXPECT_TRUE(Result.IsEquivalent);
  EXPECT_TRUE(Result.Differences.empty());
}

TEST(IREquivalenceCheckerTest, DifferentFuncCountNotEquivalent) {
  IRTypeContext Ctx;
  auto ModA = makeModuleWithFunc(Ctx, "a", "foo");
  auto ModB = std::make_unique<IRModule>("b", Ctx);
  auto Result = IREquivalenceChecker::check(*ModA, *ModB);
  EXPECT_FALSE(Result.IsEquivalent);
  EXPECT_FALSE(Result.Differences.empty());
}

TEST(IREquivalenceCheckerTest, TypeEquivalent) {
  IRTypeContext Ctx;
  auto* Int32_A = Ctx.getIntType(32);
  auto* Int32_B = Ctx.getIntType(32);
  EXPECT_TRUE(IREquivalenceChecker::isTypeEquivalent(Int32_A, Int32_B));
}

TEST(IREquivalenceCheckerTest, TypeNotEquivalent) {
  IRTypeContext Ctx;
  auto* Int32 = Ctx.getIntType(32);
  auto* Int64 = Ctx.getIntType(64);
  EXPECT_FALSE(IREquivalenceChecker::isTypeEquivalent(Int32, Int64));
}

TEST(IREquivalenceCheckerTest, NullTypeEquivalent) {
  EXPECT_TRUE(IREquivalenceChecker::isTypeEquivalent(nullptr, nullptr));
  EXPECT_FALSE(IREquivalenceChecker::isTypeEquivalent(nullptr,
      reinterpret_cast<IRType*>(1)));
}

TEST(IREquivalenceCheckerTest, StructurallyEquivalent) {
  IRTypeContext Ctx;
  auto ModA = makeModuleWithFunc(Ctx, "a", "foo");
  auto ModB = makeModuleWithFunc(Ctx, "b", "foo");
  EXPECT_TRUE(IREquivalenceChecker::isStructurallyEquivalent(
      *ModA->getFunction("foo"), *ModB->getFunction("foo")));
}
