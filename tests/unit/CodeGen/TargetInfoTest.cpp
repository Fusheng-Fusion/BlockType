//===--- TargetInfoTest.cpp - TargetInfo Unit Tests ----------*- C++ -*-===//

#include "gtest/gtest.h"
#include "blocktype/CodeGen/TargetInfo.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/TargetParser/Triple.h"

using namespace blocktype;

namespace {

//===----------------------------------------------------------------------===//
// 工厂方法测试
//===----------------------------------------------------------------------===//

TEST(TargetInfoFactoryTest, CreateX86_64Linux) {
  auto TI = TargetInfo::Create("x86_64-unknown-linux-gnu");
  ASSERT_NE(TI, nullptr);
  EXPECT_TRUE(TI->isX86_64());
  EXPECT_FALSE(TI->isAArch64());
  EXPECT_TRUE(TI->isLinux());
  EXPECT_FALSE(TI->isMacOS());
}

TEST(TargetInfoFactoryTest, CreateX86_64MacOS) {
  auto TI = TargetInfo::Create("x86_64-apple-darwin");
  ASSERT_NE(TI, nullptr);
  EXPECT_TRUE(TI->isX86_64());
  EXPECT_FALSE(TI->isAArch64());
  EXPECT_FALSE(TI->isLinux());
  EXPECT_TRUE(TI->isMacOS());
}

TEST(TargetInfoFactoryTest, CreateAArch64MacOS) {
  auto TI = TargetInfo::Create("arm64-apple-macos");
  ASSERT_NE(TI, nullptr);
  EXPECT_FALSE(TI->isX86_64());
  EXPECT_TRUE(TI->isAArch64());
  EXPECT_FALSE(TI->isLinux());
  EXPECT_TRUE(TI->isMacOS());
}

TEST(TargetInfoFactoryTest, CreateAArch64Linux) {
  auto TI = TargetInfo::Create("aarch64-unknown-linux-gnu");
  ASSERT_NE(TI, nullptr);
  EXPECT_FALSE(TI->isX86_64());
  EXPECT_TRUE(TI->isAArch64());
  EXPECT_TRUE(TI->isLinux());
  EXPECT_FALSE(TI->isMacOS());
}

TEST(TargetInfoFactoryTest, DefaultIsX86_64) {
  // Unknown triple defaults to x86_64
  auto TI = TargetInfo::Create("x86_64-unknown-unknown");
  ASSERT_NE(TI, nullptr);
  EXPECT_TRUE(TI->isX86_64());
}

//===----------------------------------------------------------------------===//
// X86_64 平台信息测试
//===----------------------------------------------------------------------===//

class X86_64TargetInfoTest : public ::testing::Test {
protected:
  std::unique_ptr<TargetInfo> LinuxTI;
  std::unique_ptr<TargetInfo> DarwinTI;

  X86_64TargetInfoTest()
      : LinuxTI(TargetInfo::Create("x86_64-unknown-linux-gnu")),
        DarwinTI(TargetInfo::Create("x86_64-apple-darwin")) {}
};

TEST_F(X86_64TargetInfoTest, LongWidth) {
  EXPECT_EQ(LinuxTI->getLongWidth(), 8u);
  EXPECT_EQ(DarwinTI->getLongWidth(), 8u);
}

TEST_F(X86_64TargetInfoTest, LongDoubleWidth) {
  // x86_64 (both Linux and Darwin): long double = 80-bit extended padded to 16 bytes
  EXPECT_EQ(LinuxTI->getLongDoubleWidth(), 16u);
  EXPECT_EQ(DarwinTI->getLongDoubleWidth(), 16u);
}

TEST_F(X86_64TargetInfoTest, BuiltinSizeLongDouble) {
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::LongDouble), 16u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::LongDouble), 16u);
}

TEST_F(X86_64TargetInfoTest, BuiltinSizeBasic) {
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Void), 0u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Bool), 1u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Char), 1u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Short), 2u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Int), 4u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Long), 8u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::LongLong), 8u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Float), 4u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Double), 8u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Int128), 16u);
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::Float128), 16u);
}

TEST_F(X86_64TargetInfoTest, PointerSize) {
  EXPECT_EQ(LinuxTI->getPointerSize(), 8u);
  EXPECT_EQ(DarwinTI->getPointerSize(), 8u);
}

TEST_F(X86_64TargetInfoTest, MaxVectorAlign) {
  EXPECT_EQ(LinuxTI->getMaxVectorAlign(), 16u);
}

TEST_F(X86_64TargetInfoTest, ThisPassedInRegister) {
  EXPECT_TRUE(LinuxTI->isThisPassedInRegister());
}

TEST_F(X86_64TargetInfoTest, DataLayoutString) {
  // x86_64 DataLayout should contain f80:128
  auto DLStr = LinuxTI->getDataLayout().getStringRepresentation();
  EXPECT_NE(DLStr.find("f80:128"), std::string::npos)
      << "x86_64 DataLayout should contain f80:128, got: " << DLStr;
}

TEST_F(X86_64TargetInfoTest, TriplePreserved) {
  EXPECT_EQ(LinuxTI->getTriple(), "x86_64-unknown-linux-gnu");
  EXPECT_EQ(DarwinTI->getTriple(), "x86_64-apple-darwin");
}

//===----------------------------------------------------------------------===//
// AArch64 平台信息测试
//===----------------------------------------------------------------------===//

class AArch64TargetInfoTest : public ::testing::Test {
protected:
  std::unique_ptr<TargetInfo> LinuxTI;
  std::unique_ptr<TargetInfo> DarwinTI;

  AArch64TargetInfoTest()
      : LinuxTI(TargetInfo::Create("aarch64-unknown-linux-gnu")),
        DarwinTI(TargetInfo::Create("arm64-apple-macos")) {}
};

TEST_F(AArch64TargetInfoTest, LongWidth) {
  EXPECT_EQ(LinuxTI->getLongWidth(), 8u);
  EXPECT_EQ(DarwinTI->getLongWidth(), 8u);
}

TEST_F(AArch64TargetInfoTest, LongDoubleWidth) {
  // Darwin AArch64: long double = double (8 bytes)
  EXPECT_EQ(DarwinTI->getLongDoubleWidth(), 8u);
  // Linux AArch64: long double = IEEE quad (16 bytes)
  EXPECT_EQ(LinuxTI->getLongDoubleWidth(), 16u);
}

TEST_F(AArch64TargetInfoTest, BuiltinSizeLongDouble) {
  // Key difference: Darwin AArch64 long double = 8 bytes
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::LongDouble), 8u);
  // Linux AArch64 long double = 16 bytes
  EXPECT_EQ(LinuxTI->getBuiltinSize(BuiltinKind::LongDouble), 16u);
}

TEST_F(AArch64TargetInfoTest, BuiltinSizeBasic) {
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Void), 0u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Bool), 1u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Int), 4u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Long), 8u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Float), 4u);
  EXPECT_EQ(DarwinTI->getBuiltinSize(BuiltinKind::Double), 8u);
}

TEST_F(AArch64TargetInfoTest, PointerSize) {
  EXPECT_EQ(DarwinTI->getPointerSize(), 8u);
  EXPECT_EQ(LinuxTI->getPointerSize(), 8u);
}

TEST_F(AArch64TargetInfoTest, MaxVectorAlign) {
  EXPECT_EQ(DarwinTI->getMaxVectorAlign(), 16u);
}

TEST_F(AArch64TargetInfoTest, ThisPassedInRegister) {
  EXPECT_TRUE(DarwinTI->isThisPassedInRegister());
}

TEST_F(AArch64TargetInfoTest, PlatformQueries) {
  EXPECT_TRUE(DarwinTI->isMacOS());
  EXPECT_FALSE(DarwinTI->isLinux());
  EXPECT_TRUE(DarwinTI->isAArch64());
  EXPECT_FALSE(DarwinTI->isX86_64());

  EXPECT_TRUE(LinuxTI->isLinux());
  EXPECT_FALSE(LinuxTI->isMacOS());
  EXPECT_TRUE(LinuxTI->isAArch64());
  EXPECT_FALSE(LinuxTI->isX86_64());
}

TEST_F(AArch64TargetInfoTest, TriplePreserved) {
  EXPECT_EQ(LinuxTI->getTriple(), "aarch64-unknown-linux-gnu");
  EXPECT_EQ(DarwinTI->getTriple(), "arm64-apple-macos");
}

//===----------------------------------------------------------------------===//
// long double 大小区别对比测试
//===----------------------------------------------------------------------===//

TEST(TargetInfoLongDoubleTest, LongDoubleDiffersAcrossPlatforms) {
  auto X86_64Linux = TargetInfo::Create("x86_64-unknown-linux-gnu");
  auto AArch64Darwin = TargetInfo::Create("arm64-apple-macos");
  auto AArch64Linux = TargetInfo::Create("aarch64-unknown-linux-gnu");

  // x86_64: long double = 16 bytes (80-bit extended padded)
  EXPECT_EQ(X86_64Linux->getBuiltinSize(BuiltinKind::LongDouble), 16u);

  // Darwin AArch64: long double = 8 bytes (same as double)
  EXPECT_EQ(AArch64Darwin->getBuiltinSize(BuiltinKind::LongDouble), 8u);

  // Linux AArch64: long double = 16 bytes (IEEE quad)
  EXPECT_EQ(AArch64Linux->getBuiltinSize(BuiltinKind::LongDouble), 16u);

  // Double is always 8 bytes
  EXPECT_EQ(X86_64Linux->getBuiltinSize(BuiltinKind::Double), 8u);
  EXPECT_EQ(AArch64Darwin->getBuiltinSize(BuiltinKind::Double), 8u);
  EXPECT_EQ(AArch64Linux->getBuiltinSize(BuiltinKind::Double), 8u);
}

} // anonymous namespace
