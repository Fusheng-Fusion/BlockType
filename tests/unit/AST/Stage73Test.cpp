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
#include "blocktype/Frontend/CompilerInvocation.h"

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

//===----------------------------------------------------------------------===//
// P7.3.2: Contract inheritance and virtual function interaction
//===----------------------------------------------------------------------===//

TEST(SemaContractInheritanceTest, CollectContractsFromFunction) {
  ASTContext Context;
  SourceLocation Loc(1);
  DiagnosticsEngine Diags;
  Sema S(Context, Diags);
  SemaCXX SCXX(S);

  // Create a function with no contracts.
  auto *FD = Context.create<FunctionDecl>(Loc, "f", Context.getIntType(),
                                            llvm::ArrayRef<ParmVarDecl *>());
  auto Contracts = SCXX.collectContracts(FD);
  EXPECT_TRUE(Contracts.empty());

  // Create a function with a pre contract.
  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *Pre = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);
  auto *Attrs = Context.create<AttributeListDecl>(Loc);
  Attrs->addContract(Pre);
  FD->setAttrs(Attrs);

  Contracts = SCXX.collectContracts(FD);
  EXPECT_EQ(Contracts.size(), 1u);
  EXPECT_TRUE(Contracts[0]->isPrecondition());
}

TEST(SemaContractInheritanceTest, InheritBaseContracts) {
  ASTContext Context;
  SourceLocation Loc(1);
  DiagnosticsEngine Diags;
  Sema S(Context, Diags);
  SemaCXX SCXX(S);

  // Create a base class with a virtual method that has a pre contract.
  auto *BaseClass = Context.create<CXXRecordDecl>(Loc, "Base");
  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *Pre = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);
  auto *BaseAttrs = Context.create<AttributeListDecl>(Loc);
  BaseAttrs->addContract(Pre);

  auto *BaseMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), BaseClass,
      nullptr, false, false, false, true /*virtual*/);
  BaseMethod->setAttrs(BaseAttrs);
  BaseClass->addMethod(BaseMethod);

  // Create a derived class with an overriding method (no own contracts).
  auto *DerivedClass = Context.create<CXXRecordDecl>(Loc, "Derived");
  QualType BaseTy = Context.getRecordType(BaseClass);
  DerivedClass->addBase(CXXRecordDecl::BaseSpecifier(
      BaseTy, Loc, false, true, 2 /*public*/));

  auto *DerivedMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), DerivedClass,
      nullptr, false, false, false, true /*virtual*/);
  DerivedClass->addMethod(DerivedMethod);

  // Inherit contracts.
  SCXX.InheritBaseContracts(DerivedMethod);

  // The derived method should now have the inherited pre contract.
  auto DerivedContracts = SCXX.collectContracts(DerivedMethod);
  EXPECT_EQ(DerivedContracts.size(), 1u);
  EXPECT_TRUE(DerivedContracts[0]->isPrecondition());
}

TEST(SemaContractInheritanceTest, NoInheritanceForNonVirtual) {
  ASTContext Context;
  SourceLocation Loc(1);
  DiagnosticsEngine Diags;
  Sema S(Context, Diags);
  SemaCXX SCXX(S);

  // Non-virtual methods should not inherit contracts.
  auto *BaseClass = Context.create<CXXRecordDecl>(Loc, "Base");
  auto *Cond = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                               Context.getIntType());
  auto *Pre = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond);
  auto *BaseAttrs = Context.create<AttributeListDecl>(Loc);
  BaseAttrs->addContract(Pre);

  auto *BaseMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), BaseClass,
      nullptr, false, false, false, true /*virtual*/);
  BaseMethod->setAttrs(BaseAttrs);
  BaseClass->addMethod(BaseMethod);

  // Non-virtual derived method.
  auto *DerivedClass = Context.create<CXXRecordDecl>(Loc, "Derived");
  QualType BaseTy = Context.getRecordType(BaseClass);
  DerivedClass->addBase(CXXRecordDecl::BaseSpecifier(
      BaseTy, Loc, false, true, 2 /*public*/));

  auto *DerivedMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), DerivedClass,
      nullptr, false, false, false, false /*NOT virtual*/);
  DerivedClass->addMethod(DerivedMethod);

  SCXX.InheritBaseContracts(DerivedMethod);

  // No contracts should be inherited for non-virtual methods.
  auto DerivedContracts = SCXX.collectContracts(DerivedMethod);
  EXPECT_TRUE(DerivedContracts.empty());
}

TEST(SemaContractInheritanceTest, DerivedOwnContractNotOverridden) {
  ASTContext Context;
  SourceLocation Loc(1);
  DiagnosticsEngine Diags;
  Sema S(Context, Diags);
  SemaCXX SCXX(S);

  // Base class with post contract.
  auto *BaseClass = Context.create<CXXRecordDecl>(Loc, "Base");
  auto *Cond1 = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                                Context.getIntType());
  auto *Post = Context.create<ContractAttr>(Loc, ContractKind::Post, Cond1);
  auto *BaseAttrs = Context.create<AttributeListDecl>(Loc);
  BaseAttrs->addContract(Post);

  auto *BaseMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), BaseClass,
      nullptr, false, false, false, true /*virtual*/);
  BaseMethod->setAttrs(BaseAttrs);
  BaseClass->addMethod(BaseMethod);

  // Derived class with own pre contract.
  auto *DerivedClass = Context.create<CXXRecordDecl>(Loc, "Derived");
  QualType BaseTy = Context.getRecordType(BaseClass);
  DerivedClass->addBase(CXXRecordDecl::BaseSpecifier(
      BaseTy, Loc, false, true, 2 /*public*/));

  auto *Cond2 = Context.create<IntegerLiteral>(Loc, llvm::APInt(32, 1),
                                                Context.getIntType());
  auto *Pre = Context.create<ContractAttr>(Loc, ContractKind::Pre, Cond2);
  auto *DerivedAttrs = Context.create<AttributeListDecl>(Loc);
  DerivedAttrs->addContract(Pre);

  auto *DerivedMethod = Context.create<CXXMethodDecl>(
      Loc, "foo", Context.getIntType(),
      llvm::ArrayRef<ParmVarDecl *>(), DerivedClass,
      nullptr, false, false, false, true /*virtual*/);
  DerivedMethod->setAttrs(DerivedAttrs);
  DerivedClass->addMethod(DerivedMethod);

  SCXX.InheritBaseContracts(DerivedMethod);

  // The derived method should have both its own pre and the inherited post.
  auto DerivedContracts = SCXX.collectContracts(DerivedMethod);
  EXPECT_EQ(DerivedContracts.size(), 2u);

  bool hasPre = false, hasPost = false;
  for (auto *CA : DerivedContracts) {
    if (CA->isPrecondition()) hasPre = true;
    if (CA->isPostcondition()) hasPost = true;
  }
  EXPECT_TRUE(hasPre);
  EXPECT_TRUE(hasPost);
}

TEST(ContractModeTest, ParseContractMode) {
  CompilerInvocation CI;

  // Test -fcontract-mode=enforce
  const char *Args1[] = {"blocktype", "-fcontract-mode=enforce"};
  EXPECT_TRUE(CI.parseCommandLine(2, Args1));
  EXPECT_EQ(CI.FrontendOpts.DefaultContractMode, ContractMode::Enforce);
  EXPECT_TRUE(CI.FrontendOpts.ContractsEnabled);

  // Test -fcontract-mode=off
  CompilerInvocation CI2;
  const char *Args2[] = {"blocktype", "-fcontract-mode=off"};
  EXPECT_TRUE(CI2.parseCommandLine(2, Args2));
  EXPECT_EQ(CI2.FrontendOpts.DefaultContractMode, ContractMode::Off);

  // Test -fcontract-mode=observe
  CompilerInvocation CI3;
  const char *Args3[] = {"blocktype", "-fcontract-mode=observe"};
  EXPECT_TRUE(CI3.parseCommandLine(2, Args3));
  EXPECT_EQ(CI3.FrontendOpts.DefaultContractMode, ContractMode::Observe);

  // Test -fcontract-mode=quick-enforce
  CompilerInvocation CI4;
  const char *Args4[] = {"blocktype", "-fcontract-mode=quick-enforce"};
  EXPECT_TRUE(CI4.parseCommandLine(2, Args4));
  EXPECT_EQ(CI4.FrontendOpts.DefaultContractMode, ContractMode::Quick_Enforce);

  // Test -fcontract-mode=default
  CompilerInvocation CI5;
  const char *Args5[] = {"blocktype", "-fcontract-mode=default"};
  EXPECT_TRUE(CI5.parseCommandLine(2, Args5));
  EXPECT_EQ(CI5.FrontendOpts.DefaultContractMode, ContractMode::Default);
}

TEST(ContractModeTest, ParseContractsEnabled) {
  // Test -fcontracts
  CompilerInvocation CI1;
  const char *Args1[] = {"blocktype", "-fcontracts"};
  EXPECT_TRUE(CI1.parseCommandLine(2, Args1));
  EXPECT_TRUE(CI1.FrontendOpts.ContractsEnabled);

  // Test -fno-contracts
  CompilerInvocation CI2;
  const char *Args2[] = {"blocktype", "-fno-contracts"};
  EXPECT_TRUE(CI2.parseCommandLine(2, Args2));
  EXPECT_FALSE(CI2.FrontendOpts.ContractsEnabled);
  EXPECT_EQ(CI2.FrontendOpts.DefaultContractMode, ContractMode::Off);
}

TEST(ContractModeTest, InvalidContractMode) {
  CompilerInvocation CI;
  const char *Args[] = {"blocktype", "-fcontract-mode=invalid"};
  EXPECT_FALSE(CI.parseCommandLine(2, Args));
}
