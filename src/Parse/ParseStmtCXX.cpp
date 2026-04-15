//===--- ParseStmtCXX.cpp - C++ Statement Parsing -----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements C++ specific statement parsing for the Parser.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Parse/Parser.h"
#include "blocktype/AST/Stmt.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

//===----------------------------------------------------------------------===//
// Try statement parsing
//===----------------------------------------------------------------------===//

Stmt *Parser::parseCXXTryStatement() {
  SourceLocation TryLoc = Tok.getLocation();
  consumeToken(); // consume 'try'

  // Parse try block
  if (!Tok.is(TokenKind::l_brace)) {
    emitError(DiagID::err_expected_lbrace);
    return Context.create<NullStmt>(TryLoc);
  }

  Stmt *TryBlock = parseCompoundStatement();
  if (!TryBlock) {
    TryBlock = Context.create<NullStmt>(TryLoc);
  }

  // Parse catch clauses
  llvm::SmallVector<Stmt *, 4> CatchStmts;
  while (Tok.is(TokenKind::kw_catch)) {
    Stmt *Catch = parseCXXCatchClause();
    if (Catch) {
      CatchStmts.push_back(Catch);
    }
  }

  // TODO: Create CXXTryStmt
  return TryBlock;
}

//===----------------------------------------------------------------------===//
// Catch clause parsing
//===----------------------------------------------------------------------===//

Stmt *Parser::parseCXXCatchClause() {
  SourceLocation CatchLoc = Tok.getLocation();
  consumeToken(); // consume 'catch'

  // Parse '('
  if (!tryConsumeToken(TokenKind::l_paren)) {
    emitError(DiagID::err_expected_lparen);
    return Context.create<NullStmt>(CatchLoc);
  }

  // Check for catch-all: catch (...)
  bool IsCatchAll = false;
  if (Tok.is(TokenKind::ellipsis)) {
    IsCatchAll = true;
    consumeToken(); // consume '...'
  } else {
    // Parse exception declaration (type name)
    // TODO: Parse type properly
    if (Tok.is(TokenKind::identifier)) {
      consumeToken(); // consume type name

      // Parse optional variable name
      if (Tok.is(TokenKind::identifier)) {
        consumeToken(); // consume variable name
      }
    }
  }

  // Parse ')'
  if (!tryConsumeToken(TokenKind::r_paren)) {
    emitError(DiagID::err_expected_rparen);
  }

  // Parse catch block
  if (!Tok.is(TokenKind::l_brace)) {
    emitError(DiagID::err_expected_lbrace);
    return Context.create<NullStmt>(CatchLoc);
  }

  Stmt *CatchBlock = parseCompoundStatement();
  if (!CatchBlock) {
    CatchBlock = Context.create<NullStmt>(CatchLoc);
  }

  // TODO: Create CXXCatchStmt
  return CatchBlock;
}

//===----------------------------------------------------------------------===//
// Coroutine statement parsing (C++20)
//===----------------------------------------------------------------------===//

Stmt *Parser::parseCoreturnStatement() {
  SourceLocation CoreturnLoc = Tok.getLocation();
  consumeToken(); // consume 'co_return'

  Expr *RetVal = nullptr;

  // Parse optional return value
  if (!Tok.is(TokenKind::semicolon)) {
    RetVal = parseExpression();
    if (!RetVal) {
      RetVal = createRecoveryExpr(CoreturnLoc);
    }
  }

  if (!tryConsumeToken(TokenKind::semicolon)) {
    emitError(DiagID::err_expected_semi);
  }

  // TODO: Create CoreturnStmt
  return Context.create<NullStmt>(CoreturnLoc);
}

Stmt *Parser::parseCoyieldStatement() {
  SourceLocation CoyieldLoc = Tok.getLocation();
  consumeToken(); // consume 'co_yield'

  Expr *Value = parseExpression();
  if (!Value) {
    Value = createRecoveryExpr(CoyieldLoc);
  }

  if (!tryConsumeToken(TokenKind::semicolon)) {
    emitError(DiagID::err_expected_semi);
  }

  // TODO: Create CoyieldStmt
  return Context.create<NullStmt>(CoyieldLoc);
}

Expr *Parser::parseCoawaitExpression() {
  SourceLocation CoawaitLoc = Tok.getLocation();
  consumeToken(); // consume 'co_await'

  Expr *Operand = parseUnaryExpression();
  if (!Operand) {
    Operand = createRecoveryExpr(CoawaitLoc);
  }

  // TODO: Create CoawaitExpr
  return Operand;
}

} // namespace blocktype
