//===--- TokenKinds.h - Token Kind Helpers --------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines helper functions for TokenKind.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/Lex/Token.h"

namespace blocktype {

/// Returns the name of the token kind (e.g., "plus", "kw_int").
const char *getTokenName(TokenKind Kind);

/// Returns true if the token is a keyword (English or Chinese).
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

/// Returns true if this is a Chinese keyword token kind.
bool isChineseKeyword(TokenKind Kind);

/// Returns the punctuation spelling for a token (e.g., "+" for plus).
/// Returns nullptr if not a punctuation token.
const char *getPunctuationSpelling(TokenKind Kind);

} // namespace blocktype
