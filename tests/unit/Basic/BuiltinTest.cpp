//===--- builtin_functions.cpp - Builtin Function Tests ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Unit tests for the Builtins infrastructure (Builtins.h/Builtins.cpp).
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include "blocktype/Basic/Builtins.h"

using namespace blocktype;

// === BuiltinID Lookup Tests ===

TEST(BuiltinsTest, IsBuiltinRecognizesKnownBuiltin) {
  EXPECT_TRUE(Builtins::isBuiltin("__builtin_abs"));
  EXPECT_TRUE(Builtins::isBuiltin("__builtin_ctz"));
  EXPECT_TRUE(Builtins::isBuiltin("__builtin_trap"));
  EXPECT_TRUE(Builtins::isBuiltin("__builtin_memcpy"));
  EXPECT_TRUE(Builtins::isBuiltin("__builtin_is_constant_evaluated"));
}

TEST(BuiltinsTest, IsBuiltinRejectsNonBuiltin) {
  EXPECT_FALSE(Builtins::isBuiltin("printf"));
  EXPECT_FALSE(Builtins::isBuiltin("abs"));
  EXPECT_FALSE(Builtins::isBuiltin("__builtin_nonexistent"));
}

TEST(BuiltinsTest, IsBuiltinRejectsNonPrefixed) {
  // Even if the name is valid, without __builtin_ prefix isBuiltin returns false
  EXPECT_FALSE(Builtins::isBuiltin("abs"));
}

TEST(BuiltinsTest, LookupReturnsCorrectID) {
  EXPECT_EQ(Builtins::lookup("__builtin_abs"), BuiltinID::__builtin_abs);
  EXPECT_EQ(Builtins::lookup("__builtin_trap"), BuiltinID::__builtin_trap);
  EXPECT_EQ(Builtins::lookup("__builtin_unreachable"), BuiltinID::__builtin_unreachable);
  EXPECT_EQ(Builtins::lookup("__builtin_expect"), BuiltinID::__builtin_expect);
}

TEST(BuiltinsTest, LookupReturnsNotBuiltinForUnknown) {
  EXPECT_EQ(Builtins::lookup("unknown_func"), BuiltinID::NotBuiltin);
  EXPECT_EQ(Builtins::lookup("__builtin_nonexistent"), BuiltinID::NotBuiltin);
}

// === BuiltinInfo Tests ===

TEST(BuiltinsTest, GetInfoReturnsCorrectName) {
  auto &Info = Builtins::getInfo(BuiltinID::__builtin_abs);
  EXPECT_STREQ(Info.Name, "__builtin_abs");
  EXPECT_STREQ(Info.Type, "i.i");
  EXPECT_STREQ(Info.Attrs, "nc");
}

TEST(BuiltinsTest, GetInfoTrapIsNoreturn) {
  auto &Info = Builtins::getInfo(BuiltinID::__builtin_trap);
  EXPECT_STREQ(Info.Name, "__builtin_trap");
  EXPECT_STREQ(Info.Attrs, "nr");
}

// === Attribute Tests ===

TEST(BuiltinsTest, IsNoThrow) {
  EXPECT_TRUE(Builtins::isNoThrow(BuiltinID::__builtin_abs));
  EXPECT_TRUE(Builtins::isNoThrow(BuiltinID::__builtin_ctz));
  EXPECT_TRUE(Builtins::isNoThrow(BuiltinID::__builtin_trap));
  // memcpy is nothrow
  EXPECT_TRUE(Builtins::isNoThrow(BuiltinID::__builtin_memcpy));
}

TEST(BuiltinsTest, IsConst) {
  EXPECT_TRUE(Builtins::isConst(BuiltinID::__builtin_abs));
  EXPECT_TRUE(Builtins::isConst(BuiltinID::__builtin_ctz));
  // trap is NOT const (it's noreturn)
  EXPECT_FALSE(Builtins::isConst(BuiltinID::__builtin_trap));
  // memcpy is NOT const (it has side effects)
  EXPECT_FALSE(Builtins::isConst(BuiltinID::__builtin_memcpy));
}

TEST(BuiltinsTest, IsNoReturn) {
  EXPECT_TRUE(Builtins::isNoReturn(BuiltinID::__builtin_trap));
  EXPECT_TRUE(Builtins::isNoReturn(BuiltinID::__builtin_unreachable));
  EXPECT_FALSE(Builtins::isNoReturn(BuiltinID::__builtin_abs));
  EXPECT_FALSE(Builtins::isNoReturn(BuiltinID::__builtin_ctz));
}

// === LLVM Intrinsic Mapping Tests ===

TEST(BuiltinsTest, LLVMIntrinsicMapping) {
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_abs), "llvm.abs");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_ctz), "llvm.cttz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_ctzl), "llvm.cttz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_ctzll), "llvm.cttz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_clz), "llvm.ctlz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_clzl), "llvm.ctlz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_clzll), "llvm.ctlz");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_popcount), "llvm.ctpop");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_popcountl), "llvm.ctpop");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_popcountll), "llvm.ctpop");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_bswap16), "llvm.bswap");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_bswap32), "llvm.bswap");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_bswap64), "llvm.bswap");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_trap), "llvm.trap");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memcpy), "llvm.memcpy");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memset), "llvm.memset");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_memmove), "llvm.memmove");
}

TEST(BuiltinsTest, LLVMIntrinsicNoMapping) {
  // expect, unreachable, is_constant_evaluated have no direct LLVM intrinsic
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_expect), "");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_unreachable), "");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_is_constant_evaluated), "");
  EXPECT_EQ(Builtins::getLLVMIntrinsicName(BuiltinID::__builtin_strlen), "");
}

// === Comprehensive Builtin Count Test ===

TEST(BuiltinsTest, AllBuiltinsAreQueryable) {
  // Verify all 22 builtins are recognized
  const char *AllBuiltins[] = {
    "__builtin_abs", "__builtin_labs", "__builtin_llabs",
    "__builtin_ctz", "__builtin_ctzl", "__builtin_ctzll",
    "__builtin_clz", "__builtin_clzl", "__builtin_clzll",
    "__builtin_popcount", "__builtin_popcountl", "__builtin_popcountll",
    "__builtin_bswap16", "__builtin_bswap32", "__builtin_bswap64",
    "__builtin_expect", "__builtin_unreachable", "__builtin_trap",
    "__builtin_strlen", "__builtin_memcpy", "__builtin_memset", "__builtin_memmove",
    "__builtin_is_constant_evaluated",
  };
  for (const auto *Name : AllBuiltins) {
    EXPECT_TRUE(Builtins::isBuiltin(Name)) << "Expected " << Name << " to be a builtin";
  }
}