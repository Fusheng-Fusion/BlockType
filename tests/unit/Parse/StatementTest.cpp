//===--- StatementTest.cpp - Statement Parsing Tests --------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include "blocktype/Parse/Parser.h"
#include "blocktype/Lex/Lexer.h"
#include "blocktype/Lex/Preprocessor.h"
#include "blocktype/Basic/SourceManager.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Expr.h"

using namespace blocktype;

namespace {

class StatementTest : public ::testing::Test {
protected:
  SourceManager SM;
  DiagnosticsEngine Diags;
  ASTContext Context;

  std::unique_ptr<Parser> parse(StringRef Code) {
    SM.createMainFileID("test.cpp", Code);
    auto PP = std::make_unique<Preprocessor>(SM, Diags);
    PP->enterSourceFile("test.cpp", Code);
    return std::make_unique<Parser>(*PP, Context);
  }
};

//===----------------------------------------------------------------------===//
// Basic Statement Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, NullStatement) {
  auto P = parse(";");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<NullStmt>(S));
}

TEST_F(StatementTest, CompoundStatement) {
  auto P = parse("{ }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CompoundStmt>(S));
  
  auto *CS = llvm::cast<CompoundStmt>(S);
  EXPECT_EQ(CS->getBody().size(), 0u);
}

TEST_F(StatementTest, CompoundStatementWithStatements) {
  auto P = parse("{ ; ; }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CompoundStmt>(S));
  
  auto *CS = llvm::cast<CompoundStmt>(S);
  EXPECT_EQ(CS->getBody().size(), 2u);
}

TEST_F(StatementTest, ReturnStatement) {
  auto P = parse("return;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ReturnStmt>(S));
  
  auto *RS = llvm::cast<ReturnStmt>(S);
  EXPECT_EQ(RS->getRetValue(), nullptr);
}

TEST_F(StatementTest, ReturnStatementWithValue) {
  auto P = parse("return 42;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ReturnStmt>(S));
  
  auto *RS = llvm::cast<ReturnStmt>(S);
  EXPECT_NE(RS->getRetValue(), nullptr);
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(RS->getRetValue()));
}

TEST_F(StatementTest, ExpressionStatement) {
  auto P = parse("42;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ExprStmt>(S));
  
  auto *ES = llvm::cast<ExprStmt>(S);
  EXPECT_NE(ES->getExpr(), nullptr);
}

//===----------------------------------------------------------------------===//
// Control Flow Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, IfStatement) {
  auto P = parse("if (true) ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<IfStmt>(S));
  
  auto *IS = llvm::cast<IfStmt>(S);
  EXPECT_NE(IS->getCond(), nullptr);
  EXPECT_NE(IS->getThen(), nullptr);
  EXPECT_EQ(IS->getElse(), nullptr);
}

TEST_F(StatementTest, IfElseStatement) {
  auto P = parse("if (true) ; else ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<IfStmt>(S));
  
  auto *IS = llvm::cast<IfStmt>(S);
  EXPECT_NE(IS->getCond(), nullptr);
  EXPECT_NE(IS->getThen(), nullptr);
  EXPECT_NE(IS->getElse(), nullptr);
}

TEST_F(StatementTest, IfWithCompoundBody) {
  auto P = parse("if (true) { }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<IfStmt>(S));
  
  auto *IS = llvm::cast<IfStmt>(S);
  EXPECT_TRUE(llvm::isa<CompoundStmt>(IS->getThen()));
}

TEST_F(StatementTest, NestedIf) {
  auto P = parse("if (a) if (b) ; else ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<IfStmt>(S));
  
  auto *Outer = llvm::cast<IfStmt>(S);
  EXPECT_TRUE(llvm::isa<IfStmt>(Outer->getThen()));
  
  auto *Inner = llvm::cast<IfStmt>(Outer->getThen());
  EXPECT_NE(Inner->getElse(), nullptr);
}

TEST_F(StatementTest, SwitchStatement) {
  auto P = parse("switch (x) { }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<SwitchStmt>(S));
  
  auto *SS = llvm::cast<SwitchStmt>(S);
  EXPECT_NE(SS->getCond(), nullptr);
}

TEST_F(StatementTest, CaseStatement) {
  auto P = parse("case 1: ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CaseStmt>(S));
  
  auto *CS = llvm::cast<CaseStmt>(S);
  EXPECT_NE(CS->getLHS(), nullptr);
}

TEST_F(StatementTest, DefaultStatement) {
  auto P = parse("default: ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<DefaultStmt>(S));
}

//===----------------------------------------------------------------------===//
// Loop Statement Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, WhileStatement) {
  auto P = parse("while (true) ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<WhileStmt>(S));
  
  auto *WS = llvm::cast<WhileStmt>(S);
  EXPECT_NE(WS->getCond(), nullptr);
  EXPECT_NE(WS->getBody(), nullptr);
}

TEST_F(StatementTest, DoWhileStatement) {
  auto P = parse("do ; while (true);");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<DoStmt>(S));
  
  auto *DS = llvm::cast<DoStmt>(S);
  EXPECT_NE(DS->getCond(), nullptr);
  EXPECT_NE(DS->getBody(), nullptr);
}

TEST_F(StatementTest, ForStatement) {
  auto P = parse("for (;;) ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ForStmt>(S));
  
  auto *FS = llvm::cast<ForStmt>(S);
  EXPECT_NE(FS->getBody(), nullptr);
}

TEST_F(StatementTest, ForStatementWithInit) {
  auto P = parse("for (int i = 0; i < 10; ++i) ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ForStmt>(S));
}

TEST_F(StatementTest, ForRangeStatement) {
  auto P = parse("for (auto x : arr) ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CXXForRangeStmt>(S));
}

//===----------------------------------------------------------------------===//
// Jump Statement Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, BreakStatement) {
  auto P = parse("break;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<BreakStmt>(S));
}

TEST_F(StatementTest, ContinueStatement) {
  auto P = parse("continue;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<ContinueStmt>(S));
}

TEST_F(StatementTest, GotoStatement) {
  auto P = parse("goto label;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<GotoStmt>(S));
}

TEST_F(StatementTest, LabelStatement) {
  auto P = parse("label: ;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<LabelStmt>(S));
  
  auto *LS = llvm::cast<LabelStmt>(S);
  EXPECT_NE(LS->getSubStmt(), nullptr);
}

//===----------------------------------------------------------------------===//
// C++ Statement Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, TryStatement) {
  auto P = parse("try { } catch (...) { }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CXXTryStmt>(S));
  
  auto *TS = llvm::cast<CXXTryStmt>(S);
  EXPECT_NE(TS->getTryBlock(), nullptr);
  EXPECT_GT(TS->getCatchBlocks().size(), 0u);
}

TEST_F(StatementTest, CatchStatement) {
  auto P = parse("try { } catch (int e) { }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CXXTryStmt>(S));
  
  auto *TS = llvm::cast<CXXTryStmt>(S);
  EXPECT_GT(TS->getCatchBlocks().size(), 0u);
}

TEST_F(StatementTest, CoreturnStatement) {
  auto P = parse("co_return;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CoreturnStmt>(S));
}

TEST_F(StatementTest, CoreturnWithValue) {
  auto P = parse("co_return 42;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CoreturnStmt>(S));
  
  auto *CRS = llvm::cast<CoreturnStmt>(S);
  EXPECT_NE(CRS->getOperand(), nullptr);
}

TEST_F(StatementTest, CoyieldStatement) {
  auto P = parse("co_yield value;");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CoyieldStmt>(S));
  
  auto *CYS = llvm::cast<CoyieldStmt>(S);
  EXPECT_NE(CYS->getOperand(), nullptr);
}

//===----------------------------------------------------------------------===//
// Nested Statement Tests
//===----------------------------------------------------------------------===//

TEST_F(StatementTest, NestedCompoundStatements) {
  auto P = parse("{ { } { } }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<CompoundStmt>(S));
  
  auto *Outer = llvm::cast<CompoundStmt>(S);
  EXPECT_EQ(Outer->getBody().size(), 2u);
  
  for (auto *Inner : Outer->getBody()) {
    EXPECT_TRUE(llvm::isa<CompoundStmt>(Inner));
  }
}

TEST_F(StatementTest, ComplexControlFlow) {
  auto P = parse("if (a) { while (b) { if (c) break; } } else { for (;;) continue; }");
  Stmt *S = P->parseStatement();
  ASSERT_NE(S, nullptr);
  EXPECT_TRUE(llvm::isa<IfStmt>(S));
}

} // anonymous namespace
