//===--- ErrorRecoveryTest.cpp - Error Recovery Tests -------------*- C++ -*-===//
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
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Stmt.h"

using namespace blocktype;

namespace {

class ErrorRecoveryTest : public ::testing::Test {
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
  
  bool hasErrors(Parser *P) {
    return P->hasErrors();
  }
};

//===----------------------------------------------------------------------===//
// Expression Error Recovery Tests
//===----------------------------------------------------------------------===//

TEST_F(ErrorRecoveryTest, MissingOperand) {
  auto P = parse("1 +");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingClosingParen) {
  auto P = parse("(1 + 2");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingOperandInTernary) {
  auto P = parse("a ? b :");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingOperandInCall) {
  auto P = parse("foo(1,)");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, ExtraOperator) {
  auto P = parse("1 + + 2");
  Expr *E = P->parseExpression();
  // May or may not have errors depending on how + is parsed
  // But should not crash
  EXPECT_NE(E, nullptr);
}

TEST_F(ErrorRecoveryTest, InvalidToken) {
  auto P = parse("1 @ 2");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

//===----------------------------------------------------------------------===//
// Statement Error Recovery Tests
//===----------------------------------------------------------------------===//

TEST_F(ErrorRecoveryTest, MissingSemicolon) {
  auto P = parse("return 42");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingClosingBrace) {
  auto P = parse("{ return; ");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingIfCondition) {
  auto P = parse("if () ;");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingWhileCondition) {
  auto P = parse("while () ;");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingForParts) {
  auto P = parse("for ( ) ;");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingSwitchCondition) {
  auto P = parse("switch () { }");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, MissingCaseValue) {
  auto P = parse("case: ;");
  (void)P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, BreakOutsideLoop) {
  // Note: Semantic analysis should catch this, not parsing
  // Parser should still parse it
  auto P = parse("break;");
  Stmt *S = P->parseStatement();
  EXPECT_NE(S, nullptr);
}

TEST_F(ErrorRecoveryTest, ContinueOutsideLoop) {
  // Note: Semantic analysis should catch this, not parsing
  auto P = parse("continue;");
  Stmt *S = P->parseStatement();
  EXPECT_NE(S, nullptr);
}

//===----------------------------------------------------------------------===//
// Recovery After Error Tests
//===----------------------------------------------------------------------===//

TEST_F(ErrorRecoveryTest, RecoveryAfterMissingSemicolon) {
  auto P = parse("{ x = 1 y = 2; }");
  Stmt *S = P->parseStatement();
  // Parser should recover and continue parsing
  EXPECT_TRUE(hasErrors(P.get()));
  EXPECT_NE(S, nullptr);
}

TEST_F(ErrorRecoveryTest, RecoveryAfterMissingBrace) {
  auto P = parse("{ if (true) { x = 1 } y = 2; }");
  Stmt *S = P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
  EXPECT_NE(S, nullptr);
}

TEST_F(ErrorRecoveryTest, MultipleErrors) {
  auto P = parse("{ x = @ y = # z = $ }");
  Stmt *S = P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
  EXPECT_NE(S, nullptr);
}

//===----------------------------------------------------------------------===//
// Edge Case Tests
//===----------------------------------------------------------------------===//

TEST_F(ErrorRecoveryTest, EmptyInput) {
  auto P = parse("");
  // Should handle gracefully
  EXPECT_NE(P.get(), nullptr);
}

TEST_F(ErrorRecoveryTest, OnlyWhitespace) {
  auto P = parse("   \t\n   ");
  EXPECT_NE(P.get(), nullptr);
}

TEST_F(ErrorRecoveryTest, DeeplyNestedErrors) {
  auto P = parse("if (a) if (b) if (c) if (d) { @ }");
  Stmt *S = P->parseStatement();
  EXPECT_TRUE(hasErrors(P.get()));
  EXPECT_NE(S, nullptr);
}

TEST_F(ErrorRecoveryTest, UnmatchedBrackets) {
  auto P = parse("(((1 + 2)");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

TEST_F(ErrorRecoveryTest, ExtraClosingBrackets) {
  auto P = parse("(1 + 2)))");
  (void)P->parseExpression();
  EXPECT_TRUE(hasErrors(P.get()));
}

} // anonymous namespace
