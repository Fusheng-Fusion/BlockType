//===--- IRConstantEmitterTest.cpp - IRConstantEmitter Unit Tests --------===//

#include <gtest/gtest.h>

#include "blocktype/Frontend/IRConstantEmitter.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRType.h"

using namespace blocktype;
using namespace blocktype::ir;
using namespace blocktype::frontend;

namespace {

class IRConstantEmitterTest : public ::testing::Test {
protected:
  IRContext IRCtx;
  IRTypeContext& TC;
  IRConstantEmitter Emitter;

  IRConstantEmitterTest()
    : TC(IRCtx.getTypeContext()), Emitter(IRCtx, TC) {}
};

} // anonymous namespace

// === V1: Integer constant (APInt version) ===
TEST_F(IRConstantEmitterTest, EmitConstIntAPInt) {
  APInt Val(32, 42);
  auto* CI = Emitter.EmitConstInt(Val);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getZExtValue(), 42u);
  EXPECT_TRUE(CI->getType()->isInteger());
}

// === V2: Integer constant (convenience version) ===
TEST_F(IRConstantEmitterTest, EmitConstIntConvenience) {
  auto* CI = Emitter.EmitConstInt(255, 8);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getZExtValue(), 255u);

  auto* CI64 = Emitter.EmitConstInt(1000, 64);
  ASSERT_NE(CI64, nullptr);
  EXPECT_EQ(CI64->getZExtValue(), 1000u);
}

// === V3: Signed integer constant ===
TEST_F(IRConstantEmitterTest, EmitConstIntSigned) {
  auto* CI = Emitter.EmitConstInt(static_cast<uint64_t>(-1), 32, true);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getSExtValue(), -1);
}

// === V4: Float constant (APFloat version) ===
TEST_F(IRConstantEmitterTest, EmitConstFloatAPFloat) {
  APFloat FVal(APFloat::Semantics::Double, 3.14);
  auto* CF = Emitter.EmitConstFloat(FVal);
  ASSERT_NE(CF, nullptr);
  EXPECT_TRUE(CF->getType()->isFloat());
  EXPECT_FALSE(CF->isNaN());
  EXPECT_FALSE(CF->isZero());
}

// === V5: Float constant (convenience version) ===
TEST_F(IRConstantEmitterTest, EmitConstFloatConvenience) {
  auto* CF32 = Emitter.EmitConstFloat(1.5, 32);
  ASSERT_NE(CF32, nullptr);
  EXPECT_TRUE(CF32->getType()->isFloat());

  auto* CF64 = Emitter.EmitConstFloat(2.718, 64);
  ASSERT_NE(CF64, nullptr);
  EXPECT_TRUE(CF64->getType()->isFloat());
}

// === V6: Special constants (null / undef / aggregate zero) ===
TEST_F(IRConstantEmitterTest, EmitSpecialConstants) {
  auto* Null = Emitter.EmitConstNull(TC.getPointerType(TC.getInt8Ty()));
  ASSERT_NE(Null, nullptr);

  auto* Undef = Emitter.EmitConstUndef(TC.getInt32Ty());
  ASSERT_NE(Undef, nullptr);

  auto* ZeroAgg = Emitter.EmitConstAggregateZero(TC.getInt32Ty());
  ASSERT_NE(ZeroAgg, nullptr);
}

// === V7: Struct constant ===
TEST_F(IRConstantEmitterTest, EmitConstStruct) {
  SmallVector<IRType*, 16> Fields;
  Fields.push_back(TC.getInt32Ty());
  Fields.push_back(TC.getInt64Ty());
  auto* STy = TC.getStructType("TestStruct", Fields);
  ASSERT_NE(STy, nullptr);

  auto* F1 = Emitter.EmitConstInt(10, 32);
  auto* F2 = Emitter.EmitConstInt(20, 64);
  SmallVector<IRConstant*, 16> Vals;
  Vals.push_back(F1);
  Vals.push_back(F2);
  auto* CS = Emitter.EmitConstStruct(STy, Vals);
  ASSERT_NE(CS, nullptr);
  EXPECT_EQ(CS->getElements().size(), 2u);
}

// === V8: Array constant ===
TEST_F(IRConstantEmitterTest, EmitConstArray) {
  auto* ATy = TC.getArrayType(TC.getInt32Ty(), 3);
  ASSERT_NE(ATy, nullptr);

  auto* E0 = Emitter.EmitConstInt(1, 32);
  auto* E1 = Emitter.EmitConstInt(2, 32);
  auto* E2 = Emitter.EmitConstInt(3, 32);
  SmallVector<IRConstant*, 16> Vals;
  Vals.push_back(E0);
  Vals.push_back(E1);
  Vals.push_back(E2);
  auto* CA = Emitter.EmitConstArray(ATy, Vals);
  ASSERT_NE(CA, nullptr);
  EXPECT_EQ(CA->getElements().size(), 3u);
}
