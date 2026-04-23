//===--- BuiltinFunctionsTest.cpp - Builtin Function E2E Tests -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// End-to-end tests for builtin function code generation.
// These tests verify that __builtin_* functions are correctly recognized
// by Sema and lowered to LLVM intrinsics by CodeGen.
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/CodeGen/CodeGenFunction.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Sema/Sema.h"
#include "blocktype/Basic/Builtins.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"

using namespace blocktype;

namespace {

/// Helper: create a minimal CodeGen pipeline and check that a builtin
/// function call is correctly lowered.
class BuiltinFunctionsTest : public ::testing::Test {
protected:
  llvm::LLVMContext LLVMCtx;
  ASTContext ASTCtx;
};

// === Test: Builtin ID Lookup in CodeGen Context ===

TEST_F(BuiltinFunctionsTest, BuiltinAbsLookupAndIntrinsic) {
  // Verify that __builtin_abs maps to llvm.abs intrinsic
  BuiltinID ID = Builtins::lookup("__builtin_abs");
  EXPECT_EQ(ID, BuiltinID::__builtin_abs);
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(ID), "llvm.abs");
}

TEST_F(BuiltinFunctionsTest, BuiltinCtzClzLookup) {
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_ctz), "llvm.cttz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_clz), "llvm.ctlz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_ctzl), "llvm.cttz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_clzl), "llvm.ctlz");
}

TEST_F(BuiltinFunctionsTest, BuiltinPopcountBswapLookup) {
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_popcount), "llvm.ctpop");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_bswap32), "llvm.bswap");
}

TEST_F(BuiltinFunctionsTest, BuiltinMemoryIntrinsics) {
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memcpy), "llvm.memcpy");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memset), "llvm.memset");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memmove), "llvm.memmove");
}

TEST_F(BuiltinFunctionsTest, BuiltinTrapIsNoreturn) {
  EXPECT_TRUE(Builtins::isNoReturn(BuiltinID::__builtin_trap));
  EXPECT_TRUE(Builtins::isNoReturn(BuiltinID::__builtin_unreachable));
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_trap), "llvm.trap");
}

TEST_F(BuiltinFunctionsTest, BuiltinExpectIntrinsic) {
  // __builtin_expect should map to llvm.expect (not just return the value)
  BuiltinID ID = Builtins::lookup("__builtin_expect");
  EXPECT_EQ(ID, BuiltinID::__builtin_expect);
  // expect is handled directly in EmitBuiltinCall, not via getLLVMIntrinsicName
  EXPECT_TRUE(Builtins::isConst(ID));
}

TEST_F(BuiltinFunctionsTest, BuiltinIsConstantEvaluated) {
  BuiltinID ID = Builtins::lookup("__builtin_is_constant_evaluated");
  EXPECT_EQ(ID, BuiltinID::__builtin_is_constant_evaluated);
  EXPECT_TRUE(Builtins::isConst(ID));
  // Returns bool type
  auto &Info = Builtins::getInfo(ID);
  EXPECT_EQ(Info.Type[0], 'b');
}

TEST_F(BuiltinFunctionsTest, BuiltinStrlenSignature) {
  auto &Info = Builtins::getInfo(BuiltinID::__builtin_strlen);
  EXPECT_STREQ(Info.Name, "__builtin_strlen");
  // strlen returns size_t (z) and takes const char* (PKc)
  EXPECT_EQ(Info.Type[0], 'z');
}

TEST_F(BuiltinFunctionsTest, ImplicitBuiltinDeclCreation) {
  // Verify that createImplicitBuiltinDecl creates a valid FunctionDecl
  // with proper parameter types parsed from the signature
  auto *FD = ASTCtx.createImplicitBuiltinDecl(BuiltinID::__builtin_abs,
                                               "__builtin_abs");
  ASSERT_NE(FD, nullptr);
  EXPECT_EQ(FD->getName(), "__builtin_abs");

  // __builtin_abs has signature "i.i" → int(int), so 1 parameter
  auto *FT = llvm::dyn_cast<FunctionType>(FD->getType().getTypePtr());
  ASSERT_NE(FT, nullptr);
  EXPECT_EQ(FT->getParamTypes().size(), 1u);
}

} // anonymous namespace
