//===--- Stage73Test.cpp - Stage 7.3 C++26 Contracts Tests ---*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests for Stage 7.3 — C++26 Contracts (P2900R14):
// - Task 7.3.1: Contract attributes (pre/post/assert)
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"
#include "blocktype/AST/Attr.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceLocation.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Sema/Sema.h"
#include "blocktype/Sema/SemaCXX.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// ContractKind and ContractMode enumerations
//===----------------------------------------------------------------------===//

TEST(ContractEnumTest, KindNames) {
  EXPECT_EQ(getContractKindName(ContractKind::Pre), "pre");
  EXPECT_EQ(getContractKindName(ContractKind::Post), "post");
  EXPECT_EQ(getContractKindName(ContractKind::Assert), "assert");
}

TEST(ContractEnumTest, ModeNames) {
  EXPECT_EQ(getContractModeName(ContractMode::Off), "off");
  EXPECT_EQ(getContractModeName(ContractMode::Default), "default");
  EXPECT_EQ(getContractModeName(ContractMode::Enforce), "enforce");
  EXPECT_EQ(getContractModeName(ContractMode::Observe), "observe");
  EXPECT_EQ(getContractModeName(ContractMode::Quick_Enforce), "quick_enforce");
}

//===----------------------------------------------------------------------===//
// ContractAttr AST node
//===----------------------------------------------------------------------===//

TEST(ContractAttrTest, Precondition) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);

  EXPECT_EQ(CA->getContractKind(), ContractKind::Pre);
  EXPECT_TRUE(CA->isPrecondition());
  EXPECT_FALSE(CA->isPostcondition());
  EXPECT_FALSE(CA->isAssertion());
  EXPECT_EQ(CA->getCondition(), Cond);
  EXPECT_EQ(CA->getContractMode(), ContractMode::Default);
  EXPECT_EQ(CA->getKind(), ASTNode::NodeKind::ContractAttrKind);
}

TEST(ContractAttrTest, Postcondition) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 0),
                                               Context.getIntType());
  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Post, Cond,
                                           ContractMode::Enforce);

  EXPECT_EQ(CA->getContractKind(), ContractKind::Post);
  EXPECT_TRUE(CA->isPostcondition());
  EXPECT_EQ(CA->getContractMode(), ContractMode::Enforce);

  CA->setContractMode(ContractMode::Observe);
  EXPECT_EQ(CA->getContractMode(), ContractMode::Observe);
}

TEST(ContractAttrTest, Assertion) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *Cond = Context.create<CXXBoolLiteral>(Loc, true, Context.getIntType());
  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Assert, Cond);

  EXPECT_EQ(CA->getContractKind(), ContractKind::Assert);
  EXPECT_TRUE(CA->isAssertion());
}

TEST(ContractAttrTest, Classof) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);

  EXPECT_TRUE(ContractAttr::classof(CA));
  EXPECT_FALSE(ContractAttr::classof(Cond));
}

TEST(ContractAttrTest, DumpDoesNotCrash) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);

  // Should not crash
  llvm::raw_null_ostream OS;
  CA->dump(OS);
}

TEST(ContractAttrTest, NullCondition) {
  ASTContext Context;
  SourceLocation Loc(1);

  auto *CA = Context.create<ContractAttr>(Loc, ContractKind::Assert, nullptr);
  EXPECT_EQ(CA->getCondition(), nullptr);
}

//===----------------------------------------------------------------------===//
// SemaCXX Contract validation
//===----------------------------------------------------------------------===//

TEST(SemaContractTest, CheckContractPlacement) {
  ASTContext Context;
  SourceLocation Loc(1);
  DiagnosticsEngine Diags;
  Sema S(Context, Diags);
  SemaCXX SCXX(S);

  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *Pre = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);
  auto *FD = Context.create<FunctionDecl>(Loc, "f", Context.getIntType(),
                                            llvm::ArrayRef<ParmVarDecl *>());

  // pre on function: valid
  EXPECT_TRUE(SCXX.CheckContractPlacement(Pre, FD));

  // pre on non-function: invalid
  auto *VD = Context.create<VarDecl>(Loc, "x", QualType(), nullptr);
  EXPECT_FALSE(SCXX.CheckContractPlacement(Pre, VD));

  // assert anywhere: valid
  auto *AssertCA = Context.create<ContractAttr>(Loc, ContractKind::Assert, Cond);
  EXPECT_TRUE(SCXX.CheckContractPlacement(AssertCA, VD));
}
