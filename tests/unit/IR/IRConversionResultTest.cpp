#include <gtest/gtest.h>
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRConversionResult.h"

using namespace blocktype;
using namespace blocktype::ir;

// V1: IRConversionResult 有效
TEST(IRConversionResultTest, ValidResult) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("test", Ctx.getTypeContext());
  IRConversionResult Result(std::move(Mod));
  EXPECT_FALSE(Result.isInvalid());
  EXPECT_TRUE(Result.isUsable());
  EXPECT_EQ(Result.getNumErrors(), 0u);
}

// V2: IRConversionResult 无效
TEST(IRConversionResultTest, InvalidResult) {
  auto InvalidResult = IRConversionResult::getInvalid(3);
  EXPECT_TRUE(InvalidResult.isInvalid());
  EXPECT_FALSE(InvalidResult.isUsable());
  EXPECT_EQ(InvalidResult.getNumErrors(), 3u);
}

// V3: IRVerificationResult 收集违规
TEST(IRConversionResultTest, VerificationViolations) {
  IRVerificationResult VR(true);
  EXPECT_TRUE(VR.isValid());
  VR.addViolation("Missing terminator in BB entry");
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getViolations().size(), 1u);
}

// V4: takeModule 转移所有权
TEST(IRConversionResultTest, TakeModule) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("owned", Ctx.getTypeContext());
  IRConversionResult Result(std::move(Mod));
  EXPECT_TRUE(Result.isUsable());
  auto Taken = Result.takeModule();
  ASSERT_NE(Taken, nullptr);
  EXPECT_EQ(Taken->getName(), "owned");
  EXPECT_FALSE(Result.isUsable());
}

// V5: 默认构造
TEST(IRConversionResultTest, DefaultConstruct) {
  IRConversionResult R;
  EXPECT_FALSE(R.isInvalid());
  EXPECT_FALSE(R.isUsable());
  EXPECT_EQ(R.getNumErrors(), 0u);
}

// V6: getInvalid 默认错误数
TEST(IRConversionResultTest, GetInvalidDefault) {
  auto R = IRConversionResult::getInvalid();
  EXPECT_TRUE(R.isInvalid());
  EXPECT_EQ(R.getNumErrors(), 1u);
}

// V7: 多条违规
TEST(IRConversionResultTest, MultipleViolations) {
  IRVerificationResult VR(true);
  VR.addViolation("Error 1");
  VR.addViolation("Error 2");
  VR.addViolation("Error 3");
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getNumViolations(), 3u);
  EXPECT_EQ(VR.getViolations()[0], "Error 1");
  EXPECT_EQ(VR.getViolations()[2], "Error 3");
}

// V8: 初始无效
TEST(IRConversionResultTest, InitiallyInvalid) {
  IRVerificationResult VR(false);
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getNumViolations(), 0u);
}

// V9: 有效模块含 TargetTriple
TEST(IRConversionResultTest, ValidModuleWithName) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("my_module", Ctx.getTypeContext(),
                                         "x86_64-linux-gnu");
  IRConversionResult Result(std::move(Mod));
  EXPECT_TRUE(Result.isUsable());
  auto M = Result.takeModule();
  ASSERT_NE(M, nullptr);
  EXPECT_EQ(M->getName(), "my_module");
  EXPECT_EQ(M->getTargetTriple(), "x86_64-linux-gnu");
}

// V10: 违规内容正确
TEST(IRConversionResultTest, ViolationContent) {
  IRVerificationResult VR(true);
  VR.addViolation("Type mismatch: expected i32, got i64");
  const auto& V = VR.getViolations();
  ASSERT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0], "Type mismatch: expected i32, got i64");
}
