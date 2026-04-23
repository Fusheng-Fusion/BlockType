//===--- CallingConventionTest.cpp - Calling Convention Tests -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests for calling convention, signext/zeroext attributes, and
// indirect call calling convention propagation.
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/CodeGen/CodeGenTypes.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/CallingConv.h"

using namespace blocktype;

namespace {

class CallingConventionTest : public ::testing::Test {
protected:
  llvm::LLVMContext LLVMCtx;
  ASTContext Ctx;
  SourceManager SM;
  std::unique_ptr<CodeGenModule> CGM;

  CallingConventionTest() {
    CGM = std::make_unique<CodeGenModule>(Ctx, LLVMCtx, SM, "test",
                                          "x86_64-apple-darwin");
  }

  /// Helper: create a FunctionDecl with given return type and param types
  FunctionDecl *makeFuncDecl(llvm::StringRef Name, QualType RetTy,
                              llvm::ArrayRef<QualType> ParamTys) {
    llvm::SmallVector<ParmVarDecl *, 4> Params;
    llvm::SmallVector<const Type *, 4> ParamRawTys;
    for (size_t I = 0; I < ParamTys.size(); ++I) {
      Params.push_back(Ctx.create<ParmVarDecl>(
          SourceLocation(I + 1), "p" + std::to_string(I), ParamTys[I],
          static_cast<unsigned>(I)));
      ParamRawTys.push_back(ParamTys[I].getTypePtr());
    }
    FunctionType *FnTy = Ctx.getFunctionType(RetTy.getTypePtr(), ParamRawTys);
    QualType FnQualTy(QualType(FnTy, Qualifier::None));
    return Ctx.create<FunctionDecl>(SourceLocation(0), Name, FnQualTy, Params);
  }
};

// === Calling Convention Tests ===

TEST_F(CallingConventionTest, FunctionHasCConv) {
  // All functions should have C calling convention
  auto *FD = makeFuncDecl("test_cc", Ctx.getVoidType(), {});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);
  EXPECT_EQ(Fn->getCallingConv(), llvm::CallingConv::C);
}

TEST_F(CallingConventionTest, FunctionWithParamsHasCConv) {
  auto *FD = makeFuncDecl("test_cc_params", Ctx.getIntType(),
                           {Ctx.getIntType(), Ctx.getLongType()});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);
  EXPECT_EQ(Fn->getCallingConv(), llvm::CallingConv::C);
}

// === signext Attribute Tests ===

TEST_F(CallingConventionTest, SignedCharParamHasSignExt) {
  // signed char (i8) on x86_64 should get signext
  QualType SCharTy = Ctx.getSCharType();
  auto *FD = makeFuncDecl("test_schar", Ctx.getIntType(), {SCharTy});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  // First argument should have signext attribute
  auto ArgIt = Fn->arg_begin();
  ASSERT_NE(ArgIt, Fn->arg_end());
  EXPECT_TRUE(ArgIt->hasAttribute(llvm::Attribute::SExt));
}

TEST_F(CallingConventionTest, SignedShortParamHasSignExt) {
  // short (i16) on x86_64 should get signext
  QualType ShortTy = Ctx.getShortType();
  auto *FD = makeFuncDecl("test_short", Ctx.getIntType(), {ShortTy});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  auto ArgIt = Fn->arg_begin();
  ASSERT_NE(ArgIt, Fn->arg_end());
  EXPECT_TRUE(ArgIt->hasAttribute(llvm::Attribute::SExt));
}

TEST_F(CallingConventionTest, IntParamNoSignExt) {
  // int (i32) on x86_64 should NOT get signext
  QualType IntTy = Ctx.getIntType();
  auto *FD = makeFuncDecl("test_int", Ctx.getIntType(), {IntTy});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  auto ArgIt = Fn->arg_begin();
  ASSERT_NE(ArgIt, Fn->arg_end());
  EXPECT_FALSE(ArgIt->hasAttribute(llvm::Attribute::SExt));
  EXPECT_FALSE(ArgIt->hasAttribute(llvm::Attribute::ZExt));
}

// === zeroext Attribute Tests ===

TEST_F(CallingConventionTest, UnsignedCharParamHasZeroExt) {
  // unsigned char on x86_64 should get zeroext
  QualType UCharTy = Ctx.getUCharType();
  auto *FD = makeFuncDecl("test_uchar", Ctx.getIntType(), {UCharTy});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  auto ArgIt = Fn->arg_begin();
  ASSERT_NE(ArgIt, Fn->arg_end());
  EXPECT_TRUE(ArgIt->hasAttribute(llvm::Attribute::ZExt));
}

TEST_F(CallingConventionTest, BoolParamHasZeroExt) {
  // bool on x86_64 should get zeroext (it's unsigned i1)
  QualType BoolTy = Ctx.getBoolType();
  auto *FD = makeFuncDecl("test_bool", Ctx.getIntType(), {BoolTy});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  auto ArgIt = Fn->arg_begin();
  ASSERT_NE(ArgIt, Fn->arg_end());
  EXPECT_TRUE(ArgIt->hasAttribute(llvm::Attribute::ZExt));
}

// === Return Value Attribute Tests ===

TEST_F(CallingConventionTest, SignedCharReturnHasSignExt) {
  // signed char return on x86_64 should get signext on return
  QualType SCharTy = Ctx.getSCharType();
  auto *FD = makeFuncDecl("test_ret_schar", SCharTy, {});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  EXPECT_TRUE(Fn->hasRetAttribute(llvm::Attribute::SExt));
}

TEST_F(CallingConventionTest, UnsignedCharReturnHasZeroExt) {
  // unsigned char return on x86_64 should get zeroext on return
  QualType UCharTy = Ctx.getUCharType();
  auto *FD = makeFuncDecl("test_ret_uchar", UCharTy, {});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  EXPECT_TRUE(Fn->hasRetAttribute(llvm::Attribute::ZExt));
}

TEST_F(CallingConventionTest, IntReturnNoSignZeroExt) {
  // int return on x86_64 should NOT get signext or zeroext
  QualType IntTy = Ctx.getIntType();
  auto *FD = makeFuncDecl("test_ret_int", IntTy, {});
  auto *Fn = CGM->GetOrCreateFunctionDecl(FD);
  ASSERT_NE(Fn, nullptr);

  EXPECT_FALSE(Fn->hasRetAttribute(llvm::Attribute::SExt));
  EXPECT_FALSE(Fn->hasRetAttribute(llvm::Attribute::ZExt));
}

// === shouldSignExtend / shouldZeroExtend Unit Tests ===

TEST_F(CallingConventionTest, ShouldSignExtendSignedChar) {
  EXPECT_TRUE(CGM->getTypes().shouldSignExtend(Ctx.getSCharType()));
  EXPECT_TRUE(CGM->getTypes().shouldSignExtend(Ctx.getShortType()));
}

TEST_F(CallingConventionTest, ShouldNotSignExtendInt) {
  EXPECT_FALSE(CGM->getTypes().shouldSignExtend(Ctx.getIntType()));
  EXPECT_FALSE(CGM->getTypes().shouldSignExtend(Ctx.getLongType()));
}

TEST_F(CallingConventionTest, ShouldZeroExtendUnsignedChar) {
  EXPECT_TRUE(CGM->getTypes().shouldZeroExtend(Ctx.getUCharType()));
  EXPECT_TRUE(CGM->getTypes().shouldZeroExtend(Ctx.getBoolType()));
}

TEST_F(CallingConventionTest, ShouldNotZeroExtendUnsignedInt) {
  EXPECT_FALSE(CGM->getTypes().shouldZeroExtend(Ctx.getUIntType()));
  EXPECT_FALSE(CGM->getTypes().shouldZeroExtend(Ctx.getULongType()));
}

// === AArch64: No signext/zeroext ===

TEST_F(CallingConventionTest, AArch64NoSignZeroExt) {
  // AArch64 target should not add signext/zeroext
  auto AArch64CGM = std::make_unique<CodeGenModule>(
      Ctx, LLVMCtx, SM, "test_aarch64", "aarch64-apple-darwin");

  QualType SCharTy = Ctx.getSCharType();
  EXPECT_FALSE(AArch64CGM->getTypes().shouldSignExtend(SCharTy));

  QualType UCharTy = Ctx.getUCharType();
  EXPECT_FALSE(AArch64CGM->getTypes().shouldZeroExtend(UCharTy));
}

} // anonymous namespace
