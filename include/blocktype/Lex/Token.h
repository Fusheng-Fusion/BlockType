//===--- Token.h - Token Interface -----------------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Token class and TokenKind enum.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/Basic/LLVM.h"
#include "blocktype/Basic/SourceLocation.h"
#include "blocktype/Basic/Language.h"

namespace blocktype {

/// TokenKind - Enumeration of all token kinds.
enum class TokenKind {
#define TOKEN(X) X,
#define KEYWORD(X, Y) kw_##X,
#define KEYWORD_ZH(X, Y) kw_##X##_zh,
#include "blocktype/Lex/TokenKinds.def"
#undef TOKEN
#undef KEYWORD
#undef KEYWORD_ZH
  NUM_TOKENS
};

/// Returns the name of the token kind.
const char *getTokenName(TokenKind Kind);

/// Returns true if the token is a keyword.
bool isKeyword(TokenKind Kind);

/// Returns true if the token is a literal (numeric, char, or string).
bool isLiteral(TokenKind Kind);

/// Returns true if the token is a string literal.
bool isStringLiteral(TokenKind Kind);

/// Returns true if the token is a character literal.
bool isCharLiteral(TokenKind Kind);

/// Returns true if the token is a numeric constant.
bool isNumericConstant(TokenKind Kind);

/// Returns true if the token is a punctuation mark.
bool isPunctuation(TokenKind Kind);

/// Returns true if the token is an assignment operator.
bool isAssignmentOperator(TokenKind Kind);

/// Returns true if the token is a comparison operator.
bool isComparisonOperator(TokenKind Kind);

/// Returns true if the token is a binary operator.
bool isBinaryOperator(TokenKind Kind);

/// Returns true if the token is a unary operator.
bool isUnaryOperator(TokenKind Kind);

/// Returns true if this is a Chinese keyword.
bool isChineseKeyword(TokenKind Kind);

/// Token - Represents a lexical token.
class Token {
  TokenKind Kind = TokenKind::unknown;
  SourceLocation Location;
  unsigned Length = 0;
  const char *LiteralData = nullptr;
  Language SourceLang = Language::English;

public:
  Token() = default;

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  TokenKind getKind() const { return Kind; }
  void setKind(TokenKind K) { Kind = K; }

  SourceLocation getLocation() const { return Location; }
  void setLocation(SourceLocation L) { Location = L; }

  unsigned getLength() const { return Length; }
  void setLength(unsigned L) { Length = L; }

  const char *getLiteralData() const { return LiteralData; }
  void setLiteralData(const char *Data) { LiteralData = Data; }

  Language getSourceLanguage() const { return SourceLang; }
  void setSourceLanguage(Language L) { SourceLang = L; }

  //===--------------------------------------------------------------------===//
  // Predicates
  //===--------------------------------------------------------------------===//

  bool is(TokenKind K) const { return Kind == K; }
  bool isNot(TokenKind K) const { return Kind != K; }

  bool isKeyword() const { return blocktype::isKeyword(Kind); }
  bool isLiteral() const { return blocktype::isLiteral(Kind); }
  bool isStringLiteral() const { return blocktype::isStringLiteral(Kind); }
  bool isCharLiteral() const { return blocktype::isCharLiteral(Kind); }
  bool isNumericConstant() const { return blocktype::isNumericConstant(Kind); }
  bool isPunctuation() const { return blocktype::isPunctuation(Kind); }

  bool isChineseKeyword() const {
    return SourceLang == Language::Chinese && isKeyword();
  }

  /// Returns the token text from the source buffer.
  StringRef getText() const {
    if (LiteralData)
      return StringRef(LiteralData, Length);
    return StringRef();
  }

  /// Clears the token to default state.
  void clear() {
    Kind = TokenKind::unknown;
    Location = SourceLocation();
    Length = 0;
    LiteralData = nullptr;
    SourceLang = Language::English;
  }

  /// Returns true if this token has valid location.
  bool hasValidLocation() const { return Location.isValid(); }

  /// Returns the end location of this token.
  SourceLocation getEndLocation() const {
    if (!Location.isValid())
      return SourceLocation();
    // Note: This assumes SourceLocation encodes offset information
    // The actual implementation depends on SourceManager
    return SourceLocation(Location.getID() + Length);
  }
};

/// IdentifierInfo - Represents information about an identifier.
/// This class is used for keyword lookup and identifier table management.
class IdentifierInfo {
  TokenKind TokenID = TokenKind::identifier;
  bool IsChinese = false;
  StringRef Name;

public:
  IdentifierInfo() = default;

  /// Returns the identifier name.
  StringRef getName() const { return Name; }
  void setName(StringRef N) { Name = N; }

  /// Returns the token kind for this identifier.
  TokenKind getTokenID() const { return TokenID; }
  void setTokenID(TokenKind ID) { TokenID = ID; }

  /// Returns true if this is a Chinese identifier.
  bool isChinese() const { return IsChinese; }
  void setChinese(bool C) { IsChinese = C; }

  /// Returns true if this identifier is a keyword.
  bool isKeyword() const { return blocktype::isKeyword(TokenID); }
};

} // namespace blocktype
