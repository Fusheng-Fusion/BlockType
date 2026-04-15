//===--- Parser.h - Parser Interface -----------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Parser class which handles parsing of C++ code.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/ASTContext.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Lex/Preprocessor.h"
#include "blocktype/Lex/Token.h"
#include "llvm/ADT/SmallVector.h"
#include <initializer_list>
#include <vector>

namespace blocktype {

class TranslationUnitDecl;

/// ParsingContext - Represents the current parsing context.
enum class ParsingContext {
  Expression,        // Parsing an expression
  Statement,         // Parsing a statement
  Declaration,       // Parsing a declaration
  MemberInitializer, // Parsing a member initializer
  TemplateArgument,  // Parsing a template argument
};

/// Parser - Parses C++ source code into an AST.
class Parser {
  Preprocessor &PP;
  ASTContext &Context;
  DiagnosticsEngine &Diags;

  // Current token lookahead
  Token Tok;      // Current token
  Token NextTok;  // Next token (for 1-token lookahead)

  // Parsing context stack
  std::vector<ParsingContext> ContextStack;

  // Error recovery state
  unsigned ErrorCount = 0;
  bool HasRecoveryExpr = false;

public:
  Parser(Preprocessor &PP, ASTContext &Ctx);
  ~Parser();

  // Non-copyable
  Parser(const Parser &) = delete;
  Parser &operator=(const Parser &) = delete;

  //===--------------------------------------------------------------------===//
  // Main entry point
  //===--------------------------------------------------------------------===//

  /// Parses a translation unit.
  TranslationUnitDecl *parseTranslationUnit();

  //===--------------------------------------------------------------------===//
  // Token operations
  //===--------------------------------------------------------------------===//

  /// Consumes the current token and advances to the next.
  void consumeToken();

  /// Tries to consume the current token if it matches the given kind.
  /// Returns true if the token was consumed.
  bool tryConsumeToken(TokenKind K);

  /// Expects the current token to be of the given kind and consumes it.
  /// Emits an error if the token doesn't match.
  void expectAndConsume(TokenKind K, const char *Msg);

  /// Returns the next token without consuming it.
  const Token &peekToken() const { return NextTok; }

  /// Returns the current token.
  const Token &currentToken() const { return Tok; }

  /// Returns true if the current token matches the given kind.
  bool isToken(TokenKind K) const { return Tok.is(K); }

  /// Returns true if the next token matches the given kind.
  bool isNextToken(TokenKind K) const { return NextTok.is(K); }

  //===--------------------------------------------------------------------===//
  // Error recovery
  //===--------------------------------------------------------------------===//

  /// Skips tokens until one of the stop tokens is found.
  void skipUntil(std::initializer_list<TokenKind> StopTokens);

  /// Emits an error at the given location.
  void emitError(SourceLocation Loc, DiagID ID);

  /// Emits an error at the current token location.
  void emitError(DiagID ID);

  /// Creates a recovery expression for error recovery.
  Expr *createRecoveryExpr(SourceLocation Loc);

  /// Returns the number of errors encountered.
  unsigned getErrorCount() const { return ErrorCount; }

  /// Returns true if any errors have been encountered.
  bool hasErrors() const { return ErrorCount > 0; }

  //===--------------------------------------------------------------------===//
  // Parsing context
  //===--------------------------------------------------------------------===//

  /// Pushes a parsing context onto the stack.
  void pushContext(ParsingContext Ctx) { ContextStack.push_back(Ctx); }

  /// Pops a parsing context from the stack.
  void popContext() {
    if (!ContextStack.empty())
      ContextStack.pop_back();
  }

  /// Returns the current parsing context.
  ParsingContext getCurrentContext() const {
    return ContextStack.empty() ? ParsingContext::Expression
                                : ContextStack.back();
  }

  /// Returns true if we're in the given context.
  bool isInContext(ParsingContext Ctx) const {
    for (const auto &C : ContextStack) {
      if (C == Ctx)
        return true;
    }
    return false;
  }

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  Preprocessor &getPreprocessor() { return PP; }
  ASTContext &getASTContext() { return Context; }
  DiagnosticsEngine &getDiagnostics() { return Diags; }

private:
  //===--------------------------------------------------------------------===//
  // Internal helpers
  //===--------------------------------------------------------------------===//

  /// Advances to the next token from the preprocessor.
  void advanceToken();

  /// Initializes the token lookahead.
  void initializeTokenLookahead();
};

} // namespace blocktype
