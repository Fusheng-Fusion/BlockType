//===--- TokenKinds.cpp - Token Kind Helpers -------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements helper functions for TokenKind.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Lex/TokenKinds.h"

namespace blocktype {

const char *getTokenName(TokenKind Kind) {
  switch (Kind) {
#define TOKEN(X) case TokenKind::X: return #X;
#define KEYWORD(X, Y) case TokenKind::kw_##X: return #X;
#define KEYWORD_ZH(X, Y) case TokenKind::kw_##X##_zh: return #X "_zh";
#include "blocktype/Lex/TokenKinds.def"
#undef TOKEN
#undef KEYWORD
#undef KEYWORD_ZH
  default:
    return "UNKNOWN";
  }
}

bool isKeyword(TokenKind Kind) {
  // Keywords start after NUM_TOKENS would be if there were no keywords
  // We check by range: all kw_* tokens are keywords
  switch (Kind) {
#define KEYWORD(X, Y) case TokenKind::kw_##X: return true;
#define KEYWORD_ZH(X, Y) case TokenKind::kw_##X##_zh: return true;
#include "blocktype/Lex/TokenKinds.def"
#undef KEYWORD
#undef KEYWORD_ZH
  default:
    return false;
  }
}

bool isLiteral(TokenKind Kind) {
  return isStringLiteral(Kind) || isCharLiteral(Kind) || 
         Kind == TokenKind::numeric_constant;
}

bool isStringLiteral(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::string_literal:
  case TokenKind::wide_string_literal:
  case TokenKind::utf8_string_literal:
  case TokenKind::utf16_string_literal:
  case TokenKind::utf32_string_literal:
    return true;
  default:
    return false;
  }
}

bool isCharLiteral(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::char_constant:
  case TokenKind::wide_char_constant:
  case TokenKind::utf8_char_constant:
  case TokenKind::utf16_char_constant:
  case TokenKind::utf32_char_constant:
    return true;
  default:
    return false;
  }
}

bool isNumericConstant(TokenKind Kind) {
  return Kind == TokenKind::numeric_constant;
}

bool isPunctuation(TokenKind Kind) {
  switch (Kind) {
  // Operators
  case TokenKind::plus:
  case TokenKind::minus:
  case TokenKind::star:
  case TokenKind::slash:
  case TokenKind::percent:
  case TokenKind::caret:
  case TokenKind::amp:
  case TokenKind::pipe:
  case TokenKind::tilde:
  case TokenKind::exclaim:
  case TokenKind::equal:
  case TokenKind::less:
  case TokenKind::greater:
  case TokenKind::plusplus:
  case TokenKind::minusminus:
  case TokenKind::lessequal:
  case TokenKind::greaterequal:
  case TokenKind::equalequal:
  case TokenKind::exclaimequal:
  case TokenKind::lessless:
  case TokenKind::greatergreater:
  case TokenKind::lesslessequal:
  case TokenKind::greatergreaterequal:
  case TokenKind::plusequal:
  case TokenKind::minusequal:
  case TokenKind::starequal:
  case TokenKind::slashequal:
  case TokenKind::percentequal:
  case TokenKind::caretequal:
  case TokenKind::ampequal:
  case TokenKind::pipeequal:
  case TokenKind::ampamp:
  case TokenKind::pipepipe:
  case TokenKind::spaceship:
  case TokenKind::hash:
  case TokenKind::hashhash:
  case TokenKind::hashat:
  // Brackets
  case TokenKind::l_paren:
  case TokenKind::r_paren:
  case TokenKind::l_brace:
  case TokenKind::r_brace:
  case TokenKind::l_square:
  case TokenKind::r_square:
  // Other punctuation
  case TokenKind::period:
  case TokenKind::ellipsis:
  case TokenKind::comma:
  case TokenKind::colon:
  case TokenKind::coloncolon:
  case TokenKind::semicolon:
  case TokenKind::question:
  case TokenKind::at:
  case TokenKind::arrow:
  case TokenKind::periodstar:
  case TokenKind::arrowstar:
  case TokenKind::dotdot:
    return true;
  default:
    return false;
  }
}

bool isAssignmentOperator(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::equal:
  case TokenKind::plusequal:
  case TokenKind::minusequal:
  case TokenKind::starequal:
  case TokenKind::slashequal:
  case TokenKind::percentequal:
  case TokenKind::caretequal:
  case TokenKind::ampequal:
  case TokenKind::pipeequal:
  case TokenKind::lesslessequal:
  case TokenKind::greatergreaterequal:
    return true;
  default:
    return false;
  }
}

bool isComparisonOperator(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::equalequal:
  case TokenKind::exclaimequal:
  case TokenKind::less:
  case TokenKind::greater:
  case TokenKind::lessequal:
  case TokenKind::greaterequal:
  case TokenKind::spaceship:
    return true;
  default:
    return false;
  }
}

bool isBinaryOperator(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::plus:
  case TokenKind::minus:
  case TokenKind::star:
  case TokenKind::slash:
  case TokenKind::percent:
  case TokenKind::caret:
  case TokenKind::amp:
  case TokenKind::pipe:
  case TokenKind::lessless:
  case TokenKind::greatergreater:
  case TokenKind::ampamp:
  case TokenKind::pipepipe:
    return true;
  default:
    return false;
  }
}

bool isUnaryOperator(TokenKind Kind) {
  switch (Kind) {
  case TokenKind::plus:
  case TokenKind::minus:
  case TokenKind::exclaim:
  case TokenKind::tilde:
  case TokenKind::star:    // dereference
  case TokenKind::amp:     // address-of
  case TokenKind::plusplus:
  case TokenKind::minusminus:
    return true;
  default:
    return false;
  }
}

bool isChineseKeyword(TokenKind Kind) {
  switch (Kind) {
#define KEYWORD_ZH(X, Y) case TokenKind::kw_##X##_zh: return true;
#include "blocktype/Lex/TokenKinds.def"
#undef KEYWORD_ZH
  default:
    return false;
  }
}

const char *getPunctuationSpelling(TokenKind Kind) {
  switch (Kind) {
  // Arithmetic
  case TokenKind::plus: return "+";
  case TokenKind::minus: return "-";
  case TokenKind::star: return "*";
  case TokenKind::slash: return "/";
  case TokenKind::percent: return "%";
  // Bitwise
  case TokenKind::caret: return "^";
  case TokenKind::amp: return "&";
  case TokenKind::pipe: return "|";
  case TokenKind::tilde: return "~";
  // Logical
  case TokenKind::exclaim: return "!";
  case TokenKind::ampamp: return "&&";
  case TokenKind::pipepipe: return "||";
  // Comparison
  case TokenKind::less: return "<";
  case TokenKind::greater: return ">";
  case TokenKind::lessequal: return "<=";
  case TokenKind::greaterequal: return ">=";
  case TokenKind::equalequal: return "==";
  case TokenKind::exclaimequal: return "!=";
  case TokenKind::spaceship: return "<=>";
  // Assignment
  case TokenKind::equal: return "=";
  case TokenKind::plusequal: return "+=";
  case TokenKind::minusequal: return "-=";
  case TokenKind::starequal: return "*=";
  case TokenKind::slashequal: return "/=";
  case TokenKind::percentequal: return "%=";
  case TokenKind::caretequal: return "^=";
  case TokenKind::ampequal: return "&=";
  case TokenKind::pipeequal: return "|=";
  case TokenKind::lesslessequal: return "<<=";
  case TokenKind::greatergreaterequal: return ">>=";
  // Shift
  case TokenKind::lessless: return "<<";
  case TokenKind::greatergreater: return ">>";
  // Increment/Decrement
  case TokenKind::plusplus: return "++";
  case TokenKind::minusminus: return "--";
  // Member access
  case TokenKind::period: return ".";
  case TokenKind::arrow: return "->";
  case TokenKind::periodstar: return ".*";
  case TokenKind::arrowstar: return "->*";
  // Conditional
  case TokenKind::question: return "?";
  case TokenKind::colon: return ":";
  case TokenKind::coloncolon: return "::";
  // Brackets
  case TokenKind::l_paren: return "(";
  case TokenKind::r_paren: return ")";
  case TokenKind::l_brace: return "{";
  case TokenKind::r_brace: return "}";
  case TokenKind::l_square: return "[";
  case TokenKind::r_square: return "]";
  // Other
  case TokenKind::ellipsis: return "...";
  case TokenKind::comma: return ",";
  case TokenKind::semicolon: return ";";
  case TokenKind::at: return "@";
  case TokenKind::hash: return "#";
  case TokenKind::hashhash: return "##";
  case TokenKind::hashat: return "#@";
  case TokenKind::dotdot: return "..";
  default:
    return nullptr;
  }
}

} // namespace blocktype
