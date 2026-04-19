//===--- TemplateInstantiationTest.cpp - Template Instantiation Tests -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/Basic/Diagnostics.h"
#include "gtest/gtest.h"

using namespace blocktype;

namespace {

class TemplateInstantiationTest : public ::testing::Test {
protected:
  void SetUp() override {
    Diags = std::make_unique<DiagnosticsEngine>();
    Context = std::make_unique<ASTContext>();
    SemaRef = std::make_unique<Sema>(*Context, *Diags);
  }

  std::unique_ptr<DiagnosticsEngine> Diags;
  std::unique_ptr<ASTContext> Context;
  std::unique_ptr<Sema> SemaRef;
};

// --- Basic Template Instantiator Tests ---

TEST_F(TemplateInstantiationTest, GetTemplateInstantiator) {
  // Test that we can get the TemplateInstantiator
  auto &Inst = SemaRef->getTemplateInstantiator();
  EXPECT_NE(&Inst, nullptr);
}

TEST_F(TemplateInstantiationTest, InstantiateFunctionTemplateNullptr) {
  auto &Inst = SemaRef->getTemplateInstantiator();
  
  // Test with null template - should return nullptr gracefully
  llvm::SmallVector<TemplateArgument, 2> Args;
  auto *FuncDecl = Inst.InstantiateFunctionTemplate(nullptr, Args);
  EXPECT_EQ(FuncDecl, nullptr);
}

} // namespace
