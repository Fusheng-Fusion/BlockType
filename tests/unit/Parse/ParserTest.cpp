//===--- ParserTest.cpp - Parser Expression Tests -----------------*- C++ -*-===//
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
#include "llvm/Support/raw_ostream.h"

using namespace blocktype;

namespace {

class ParserTest : public ::testing::Test {
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
// Literal Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, IntegerLiteral) {
  auto P = parse("42");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(E));
  
  auto *Lit = llvm::cast<IntegerLiteral>(E);
  EXPECT_EQ(Lit->getValue(), 42);
}

TEST_F(ParserTest, IntegerLiteralHex) {
  auto P = parse("0xFF");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(E));
  
  auto *Lit = llvm::cast<IntegerLiteral>(E);
  EXPECT_EQ(Lit->getValue(), 255);
}

TEST_F(ParserTest, IntegerLiteralBinary) {
  auto P = parse("0b1010");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(E));
  
  auto *Lit = llvm::cast<IntegerLiteral>(E);
  EXPECT_EQ(Lit->getValue(), 10);
}

TEST_F(ParserTest, FloatingLiteral) {
  auto P = parse("3.14");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<FloatingLiteral>(E));
}

TEST_F(ParserTest, FloatingLiteralScientific) {
  auto P = parse("1.5e10");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<FloatingLiteral>(E));
}

TEST_F(ParserTest, StringLiteral) {
  auto P = parse("\"hello\"");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<StringLiteral>(E));
  
  auto *Lit = llvm::cast<StringLiteral>(E);
  EXPECT_EQ(Lit->getValue(), "hello");
}

TEST_F(ParserTest, CharacterLiteral) {
  auto P = parse("'a'");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CharacterLiteral>(E));
  
  auto *Lit = llvm::cast<CharacterLiteral>(E);
  EXPECT_EQ(Lit->getValue(), 'a');
}

TEST_F(ParserTest, BoolLiteralTrue) {
  auto P = parse("true");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CXXBoolLiteral>(E));
  
  auto *Lit = llvm::cast<CXXBoolLiteral>(E);
  EXPECT_TRUE(Lit->getValue());
}

TEST_F(ParserTest, BoolLiteralFalse) {
  auto P = parse("false");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CXXBoolLiteral>(E));
  
  auto *Lit = llvm::cast<CXXBoolLiteral>(E);
  EXPECT_FALSE(Lit->getValue());
}

TEST_F(ParserTest, NullPtrLiteral) {
  auto P = parse("nullptr");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CXXNullPtrLiteral>(E));
}

//===----------------------------------------------------------------------===//
// Binary Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, BinaryAdd) {
  auto P = parse("1 + 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Add);
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(BinOp->getLHS()));
  EXPECT_TRUE(llvm::isa<IntegerLiteral>(BinOp->getRHS()));
}

TEST_F(ParserTest, BinarySubtract) {
  auto P = parse("5 - 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Sub);
}

TEST_F(ParserTest, BinaryMultiply) {
  auto P = parse("2 * 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Mul);
}

TEST_F(ParserTest, BinaryDivide) {
  auto P = parse("6 / 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Div);
}

TEST_F(ParserTest, BinaryModulo) {
  auto P = parse("7 % 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Rem);
}

TEST_F(ParserTest, Precedence) {
  // 1 + 2 * 3 should parse as 1 + (2 * 3)
  auto P = parse("1 + 2 * 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Add);
  
  // RHS should be a multiplication
  EXPECT_TRUE(llvm::isa<BinaryOperator>(BinOp->getRHS()));
  auto *RHS = llvm::cast<BinaryOperator>(BinOp->getRHS());
  EXPECT_EQ(RHS->getOpcode(), BinaryOpKind::Mul);
}

TEST_F(ParserTest, LeftAssociativity) {
  // 1 - 2 - 3 should parse as (1 - 2) - 3
  auto P = parse("1 - 2 - 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Sub);
  
  // LHS should be a subtraction
  EXPECT_TRUE(llvm::isa<BinaryOperator>(BinOp->getLHS()));
  auto *LHS = llvm::cast<BinaryOperator>(BinOp->getLHS());
  EXPECT_EQ(LHS->getOpcode(), BinaryOpKind::Sub);
}

TEST_F(ParserTest, Parentheses) {
  // (1 + 2) * 3 should parse as (1 + 2) * 3
  auto P = parse("(1 + 2) * 3");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Mul);
  
  // LHS should be a parenthesized addition
  EXPECT_TRUE(llvm::isa<BinaryOperator>(BinOp->getLHS()));
  auto *LHS = llvm::cast<BinaryOperator>(BinOp->getLHS());
  EXPECT_EQ(LHS->getOpcode(), BinaryOpKind::Add);
}

//===----------------------------------------------------------------------===//
// Unary Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, UnaryPlus) {
  auto P = parse("+5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::Plus);
}

TEST_F(ParserTest, UnaryMinus) {
  auto P = parse("-5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::Minus);
}

TEST_F(ParserTest, UnaryNot) {
  auto P = parse("!true");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::LNot);
}

TEST_F(ParserTest, UnaryComplement) {
  auto P = parse("~0xFF");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::Not);
}

TEST_F(ParserTest, UnaryDereference) {
  auto P = parse("*ptr");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::Deref);
}

TEST_F(ParserTest, UnaryAddressOf) {
  auto P = parse("&x");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::AddrOf);
}

TEST_F(ParserTest, PreIncrement) {
  auto P = parse("++i");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::PreInc);
  EXPECT_TRUE(UnOp->isPrefix());
}

TEST_F(ParserTest, PreDecrement) {
  auto P = parse("--i");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::PreDec);
  EXPECT_TRUE(UnOp->isPrefix());
}

TEST_F(ParserTest, PostIncrement) {
  auto P = parse("i++");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::PostInc);
  EXPECT_FALSE(UnOp->isPrefix());
}

TEST_F(ParserTest, PostDecrement) {
  auto P = parse("i--");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<UnaryOperator>(E));
  
  auto *UnOp = llvm::cast<UnaryOperator>(E);
  EXPECT_EQ(UnOp->getOpcode(), UnaryOpKind::PostDec);
  EXPECT_FALSE(UnOp->isPrefix());
}

//===----------------------------------------------------------------------===//
// Comparison Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, Equal) {
  auto P = parse("a == b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::EQ);
}

TEST_F(ParserTest, NotEqual) {
  auto P = parse("a != b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::NE);
}

TEST_F(ParserTest, Less) {
  auto P = parse("a < b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::LT);
}

TEST_F(ParserTest, LessEqual) {
  auto P = parse("a <= b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::LE);
}

TEST_F(ParserTest, Greater) {
  auto P = parse("a > b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::GT);
}

TEST_F(ParserTest, GreaterEqual) {
  auto P = parse("a >= b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::GE);
}

//===----------------------------------------------------------------------===//
// Logical Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, LogicalAnd) {
  auto P = parse("a && b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::LAnd);
}

TEST_F(ParserTest, LogicalOr) {
  auto P = parse("a || b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::LOr);
}

//===----------------------------------------------------------------------===//
// Bitwise Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, BitwiseAnd) {
  auto P = parse("a & b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::And);
}

TEST_F(ParserTest, BitwiseOr) {
  auto P = parse("a | b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Or);
}

TEST_F(ParserTest, BitwiseXor) {
  auto P = parse("a ^ b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Xor);
}

TEST_F(ParserTest, LeftShift) {
  auto P = parse("a << 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Shl);
}

TEST_F(ParserTest, RightShift) {
  auto P = parse("a >> 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Shr);
}

//===----------------------------------------------------------------------===//
// Assignment Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, SimpleAssignment) {
  auto P = parse("x = 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Assign);
}

TEST_F(ParserTest, AddAssignment) {
  auto P = parse("x += 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::AddAssign);
}

TEST_F(ParserTest, SubAssignment) {
  auto P = parse("x -= 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::SubAssign);
}

TEST_F(ParserTest, MulAssignment) {
  auto P = parse("x *= 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::MulAssign);
}

TEST_F(ParserTest, DivAssignment) {
  auto P = parse("x /= 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::DivAssign);
}

TEST_F(ParserTest, ModAssignment) {
  auto P = parse("x %= 5");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::RemAssign);
}

TEST_F(ParserTest, AndAssignment) {
  auto P = parse("x &= mask");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::AndAssign);
}

TEST_F(ParserTest, OrAssignment) {
  auto P = parse("x |= mask");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::OrAssign);
}

TEST_F(ParserTest, XorAssignment) {
  auto P = parse("x ^= mask");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::XorAssign);
}

TEST_F(ParserTest, ShlAssignment) {
  auto P = parse("x <<= 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::ShlAssign);
}

TEST_F(ParserTest, ShrAssignment) {
  auto P = parse("x >>= 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::ShrAssign);
}

//===----------------------------------------------------------------------===//
// Conditional Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ConditionalOperator) {
  auto P = parse("cond ? a : b");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<ConditionalOperator>(E));
  
  auto *CondOp = llvm::cast<ConditionalOperator>(E);
  EXPECT_NE(CondOp->getCond(), nullptr);
  EXPECT_NE(CondOp->getTrueExpr(), nullptr);
  EXPECT_NE(CondOp->getFalseExpr(), nullptr);
}

TEST_F(ParserTest, NestedConditional) {
  auto P = parse("a ? b ? c : d : e");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<ConditionalOperator>(E));
  
  auto *Outer = llvm::cast<ConditionalOperator>(E);
  EXPECT_TRUE(llvm::isa<ConditionalOperator>(Outer->getTrueExpr()));
}

//===----------------------------------------------------------------------===//
// Comma Operator Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, CommaOperator) {
  auto P = parse("a, b, c");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::Comma);
}

//===----------------------------------------------------------------------===//
// Identifier and Call Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, Identifier) {
  auto P = parse("foo");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<DeclRefExpr>(E));
  
  auto *DRE = llvm::cast<DeclRefExpr>(E);
  EXPECT_NE(DRE->getDecl(), nullptr);
}

TEST_F(ParserTest, CallExpression) {
  auto P = parse("foo()");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CallExpr>(E));
  
  auto *Call = llvm::cast<CallExpr>(E);
  EXPECT_EQ(Call->getNumArgs(), 0u);
}

TEST_F(ParserTest, CallWithArgs) {
  auto P = parse("foo(1, 2, 3)");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CallExpr>(E));
  
  auto *Call = llvm::cast<CallExpr>(E);
  EXPECT_EQ(Call->getNumArgs(), 3u);
}

TEST_F(ParserTest, NestedCall) {
  auto P = parse("foo(bar())");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<CallExpr>(E));
  
  auto *Call = llvm::cast<CallExpr>(E);
  EXPECT_EQ(Call->getNumArgs(), 1u);
  EXPECT_TRUE(llvm::isa<CallExpr>(Call->getArgs()[0]));
}

//===----------------------------------------------------------------------===//
// Complex Expression Tests
//===----------------------------------------------------------------------===//

TEST_F(ParserTest, ComplexExpression1) {
  // a + b * c - d / e
  auto P = parse("a + b * c - d / e");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
}

TEST_F(ParserTest, ComplexExpression2) {
  // (a || b) && (c || d)
  auto P = parse("(a || b) && (c || d)");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
  
  auto *BinOp = llvm::cast<BinaryOperator>(E);
  EXPECT_EQ(BinOp->getOpcode(), BinaryOpKind::LAnd);
}

TEST_F(ParserTest, DeeplyNested) {
  // (((((1))))) + 2
  auto P = parse("(((((1))))) + 2");
  Expr *E = P->parseExpression();
  ASSERT_NE(E, nullptr);
  EXPECT_TRUE(llvm::isa<BinaryOperator>(E));
}

} // anonymous namespace
