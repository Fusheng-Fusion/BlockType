//===--- ASTToIRConverterTest.cpp - ASTToIRConverter Tests ---*- C++ -*-===//

#include <gtest/gtest.h>

#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/ASTToIRConverter.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/TargetLayout.h"

using namespace blocktype;
using namespace blocktype::frontend;

namespace {

/// Helper: create a basic test environment
struct TestEnv {
  ir::IRContext IRCtx;
  ir::IRTypeContext& TypeCtx;
  std::unique_ptr<ir::TargetLayout> Layout;
  DiagnosticsEngine Diags;

  TestEnv()
    : TypeCtx(IRCtx.getTypeContext()),
      Layout(ir::TargetLayout::Create("x86_64-unknown-linux-gnu")),
      Diags() {}
};

} // anonymous namespace

// === Test V1: Empty TU conversion ===
TEST(ASTToIRConverterTest, EmptyTU) {
  TestEnv Env;

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);

  TranslationUnitDecl TU{SourceLocation()};
  auto Result = Converter.convert(&TU);

  EXPECT_TRUE(Result.isUsable());
  ASSERT_NE(Result.getModule(), nullptr);
  EXPECT_EQ(Result.getModule()->getNumFunctions(), 0u);
}

// === Test V2: TU with a simple function ===
TEST(ASTToIRConverterTest, TUWithFunction) {
  TestEnv Env;

  // Create a simple function: void main() {}
  QualType VoidTy;
  CompoundStmt EmptyBody(SourceLocation(), {});
  FunctionDecl MainFD(SourceLocation(), "main", VoidTy, {}, &EmptyBody);

  TranslationUnitDecl TU{SourceLocation()};
  TU.addDecl(&MainFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  EXPECT_TRUE(Result.isUsable());
  ASSERT_NE(Result.getModule(), nullptr);
  auto* MainFn = Result.getModule()->getFunction("main");
  EXPECT_NE(MainFn, nullptr);
}

// === Test V3: TU with global variable ===
TEST(ASTToIRConverterTest, TUWithGlobalVar) {
  TestEnv Env;

  // Create a global variable: int x;
  QualType IntTy;
  VarDecl GlobalVar(SourceLocation(), "x", IntTy, nullptr, false, false);

  TranslationUnitDecl TU{SourceLocation()};
  TU.addDecl(&GlobalVar);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  EXPECT_TRUE(Result.isUsable());
  ASSERT_NE(Result.getModule(), nullptr);
  auto* GV = Result.getModule()->getGlobalVariable("x");
  EXPECT_NE(GV, nullptr);
}

// === Test V4: Error recovery - declaration-only function ===
TEST(ASTToIRConverterTest, ErrorRecoveryDeclOnly) {
  TestEnv Env;

  // Create a function declaration only (no body)
  QualType VoidTy;
  FunctionDecl DeclFD(SourceLocation(), "decl_only", VoidTy, {}, nullptr);

  TranslationUnitDecl TU{SourceLocation()};
  TU.addDecl(&DeclFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  // Should still be usable (declaration-only functions are fine)
  EXPECT_TRUE(Result.isUsable());
  auto* Fn = Result.getModule()->getFunction("decl_only");
  ASSERT_NE(Fn, nullptr);
  EXPECT_TRUE(Fn->isDeclaration());  // No body
}

// === Test V5: Multiple functions and globals ===
TEST(ASTToIRConverterTest, MixedDecls) {
  TestEnv Env;

  QualType VoidTy;

  // Create two functions
  CompoundStmt EmptyBody(SourceLocation(), {});
  FunctionDecl FooFD(SourceLocation(), "foo", VoidTy, {}, &EmptyBody);
  FunctionDecl BarFD(SourceLocation(), "bar", VoidTy, {}, &EmptyBody);

  // Create a global variable
  QualType IntTy;
  VarDecl GlobalVar(SourceLocation(), "g_counter", IntTy, nullptr, false, false);

  TranslationUnitDecl TU{SourceLocation()};
  TU.addDecl(&FooFD);
  TU.addDecl(&GlobalVar);
  TU.addDecl(&BarFD);

  ASTToIRConverter Converter(Env.IRCtx, Env.TypeCtx, *Env.Layout, Env.Diags);
  auto Result = Converter.convert(&TU);

  EXPECT_TRUE(Result.isUsable());
  ASSERT_NE(Result.getModule(), nullptr);
  EXPECT_NE(Result.getModule()->getFunction("foo"), nullptr);
  EXPECT_NE(Result.getModule()->getFunction("bar"), nullptr);
  EXPECT_NE(Result.getModule()->getGlobalVariable("g_counter"), nullptr);
}

// === Test V6: IRConversionResult extended API ===
TEST(ASTToIRConverterTest, ConversionResultAPI) {
  ir::IRConversionResult R;
  EXPECT_FALSE(R.isInvalid());
  EXPECT_EQ(R.getNumErrors(), 0u);

  R.addError();
  EXPECT_TRUE(R.isInvalid());
  EXPECT_EQ(R.getNumErrors(), 1u);

  R.addError();
  EXPECT_EQ(R.getNumErrors(), 2u);

  // Test getModule() on empty result
  EXPECT_EQ(R.getModule(), nullptr);
}

// === Test V7: TargetLayout getTriple() ===
TEST(ASTToIRConverterTest, TargetLayoutGetTriple) {
  auto Layout = ir::TargetLayout::Create("x86_64-unknown-linux-gnu");
  EXPECT_EQ(Layout->getTriple(), "x86_64-unknown-linux-gnu");
}

// === Test V8: VarDecl hasGlobalStorage() ===
TEST(ASTToIRConverterTest, VarDeclHasGlobalStorage) {
  QualType IntTy;
  VarDecl GlobalVar(SourceLocation(), "g", IntTy);
  EXPECT_TRUE(GlobalVar.hasGlobalStorage());
}

// === Test V9: CXXRecordDecl hasVTable() ===
TEST(ASTToIRConverterTest, CXXRecordDeclHasVTable) {
  CXXRecordDecl RD(SourceLocation(), "MyClass");
  EXPECT_FALSE(RD.hasVTable());  // No methods, no vtable
}
