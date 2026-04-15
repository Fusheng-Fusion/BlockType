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
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Stmt.h"

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
  if (TryBlock == nullptr) {
    TryBlock = Context.create<NullStmt>(TryLoc);
  }

  // Parse catch clauses
  llvm::SmallVector<Stmt *, 4> CatchStmts;
  while (Tok.is(TokenKind::kw_catch)) {
    Stmt *Catch = parseCXXCatchClause();
    if (Catch != nullptr) {
      CatchStmts.push_back(Catch);
    }
  }

  return Context.create<CXXTryStmt>(TryLoc, TryBlock, CatchStmts);
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
  VarDecl *ExceptionDecl = nullptr;
  if (Tok.is(TokenKind::ellipsis)) {
    consumeToken(); // consume '...'
    // catch-all: ExceptionDecl is nullptr
  } else {
    // Parse exception declaration (type name)
    QualType ExceptionType = parseType();
    if (ExceptionType.isNull()) {
      emitError(DiagID::err_expected_type);
    } else {
      // Parse optional variable name
      llvm::StringRef VarName;
      SourceLocation VarLoc;
      if (Tok.is(TokenKind::identifier)) {
        VarName = Tok.getText();
        VarLoc = Tok.getLocation();
        consumeToken();
      }
      // Create VarDecl for exception declaration
      ExceptionDecl = Context.create<VarDecl>(VarLoc, VarName, ExceptionType, nullptr);
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
  if (CatchBlock == nullptr) {
    CatchBlock = Context.create<NullStmt>(CatchLoc);
  }

  return Context.create<CXXCatchStmt>(CatchLoc, ExceptionDecl, CatchBlock);
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
    if (RetVal == nullptr) {
      RetVal = createRecoveryExpr(CoreturnLoc);
    }
  }

  if (!tryConsumeToken(TokenKind::semicolon)) {
    emitError(DiagID::err_expected_semi);
  }

  return Context.create<CoreturnStmt>(CoreturnLoc, RetVal);
}

Stmt *Parser::parseCoyieldStatement() {
  SourceLocation CoyieldLoc = Tok.getLocation();
  consumeToken(); // consume 'co_yield'

  Expr *Value = parseExpression();
  if (Value == nullptr) {
    Value = createRecoveryExpr(CoyieldLoc);
  }

  if (!tryConsumeToken(TokenKind::semicolon)) {
    emitError(DiagID::err_expected_semi);
  }

  return Context.create<CoyieldStmt>(CoyieldLoc, Value);
}

Expr *Parser::parseCoawaitExpression() {
  SourceLocation CoawaitLoc = Tok.getLocation();
  consumeToken(); // consume 'co_await'

  Expr *Operand = parseUnaryExpression();
  if (Operand == nullptr) {
    Operand = createRecoveryExpr(CoawaitLoc);
  }

  return Context.create<CoawaitExpr>(CoawaitLoc, Operand);
}

} // namespace blocktype
