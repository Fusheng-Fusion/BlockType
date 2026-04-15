//===--- ParseType.cpp - Type Parsing -----------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements type parsing for the BlockType parser.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Parse/Parser.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Lex/Token.h"
#include "llvm/ADT/SmallVector.h"

namespace blocktype {

using namespace blocktype;

//===----------------------------------------------------------------------===//
// Type Parsing
//===----------------------------------------------------------------------===//

/// parseType - Parse a complete type.
///
/// type ::= type-specifier abstract-declarator?
///
QualType Parser::parseType() {
  // Parse type specifier
  QualType Result = parseTypeSpecifier();
  if (Result.isNull())
    return QualType();
  
  // Parse declarator (pointers, references, arrays, functions)
  Result = parseDeclarator(Result);
  
  return Result;
}

/// parseTypeSpecifier - Parse a type specifier.
///
/// type-specifier ::= builtin-type
///                  | named-type
///                  | 'const' type-specifier
///                  | 'volatile' type-specifier
///
QualType Parser::parseTypeSpecifier() {
  QualType Result;
  Qualifier Quals = Qualifier::None;
  
  // Parse CVR qualifiers
  while (true) {
    if (Tok.is(TokenKind::kw_const)) {
      Quals = Quals | Qualifier::Const;
      consumeToken();
    } else if (Tok.is(TokenKind::kw_volatile)) {
      Quals = Quals | Qualifier::Volatile;
      consumeToken();
    } else if (Tok.is(TokenKind::kw_restrict)) {
      Quals = Quals | Qualifier::Restrict;
      consumeToken();
    } else {
      break;
    }
  }
  
  // Parse the base type
  Result = parseBuiltinType();
  if (Result.isNull()) {
    // Try to parse a named type
    if (Tok.is(TokenKind::identifier)) {
      // TODO: Look up the type name in the symbol table
      // For now, create an unresolved type
      // This should be replaced with proper type lookup
      emitError(DiagID::err_expected_type);
      consumeToken();
      return QualType();
    }
    return QualType();
  }
  
  // Apply qualifiers
  if (Quals != Qualifier::None) {
    Result = QualType(Result.getTypePtr(), Quals);
  }
  
  return Result;
}

/// parseBuiltinType - Parse a builtin type.
///
/// builtin-type ::= 'void'
///               | 'bool'
///               | 'char' ('signed' | 'unsigned')?
///               | 'short' 'int'?
///               | 'int' ('signed' | 'unsigned')?
///               | 'long' 'int'?
///               | 'float'
///               | 'double'
///               | 'auto'
///
QualType Parser::parseBuiltinType() {
  BuiltinKind Kind = BuiltinKind::NumBuiltinTypes;
  SourceLocation Loc = Tok.getLocation();
  
  switch (Tok.getKind()) {
  case TokenKind::kw_void:
    Kind = BuiltinKind::Void;
    consumeToken();
    break;
    
  case TokenKind::kw_bool:
    Kind = BuiltinKind::Bool;
    consumeToken();
    break;
    
  case TokenKind::kw_char:
    Kind = BuiltinKind::Char;
    consumeToken();
    // Check for signed/unsigned
    if (Tok.is(TokenKind::kw_signed)) {
      consumeToken();
    } else if (Tok.is(TokenKind::kw_unsigned)) {
      Kind = BuiltinKind::UnsignedChar;
      consumeToken();
    }
    break;
    
  case TokenKind::kw_short:
    consumeToken();
    // Optional 'int'
    if (Tok.is(TokenKind::kw_int))
      consumeToken();
    Kind = BuiltinKind::Short;
    break;
    
  case TokenKind::kw_int:
    Kind = BuiltinKind::Int;
    consumeToken();
    break;
    
  case TokenKind::kw_long:
    consumeToken();
    // Check for 'long long'
    if (Tok.is(TokenKind::kw_long)) {
      consumeToken();
      Kind = BuiltinKind::LongLong;
    } else {
      Kind = BuiltinKind::Long;
    }
    // Optional 'int'
    if (Tok.is(TokenKind::kw_int))
      consumeToken();
    break;
    
  case TokenKind::kw_float:
    Kind = BuiltinKind::Float;
    consumeToken();
    break;
    
  case TokenKind::kw_double:
    Kind = BuiltinKind::Double;
    consumeToken();
    break;
    
  case TokenKind::kw_auto:
    // TODO: Implement auto type deduction
    // For now, treat as an error
    emitError(DiagID::err_expected_type);
    consumeToken();
    return QualType();
    
  case TokenKind::kw_signed:
    consumeToken();
    if (Tok.is(TokenKind::kw_char)) {
      Kind = BuiltinKind::Char; // signed char
      consumeToken();
    } else if (Tok.is(TokenKind::kw_short)) {
      consumeToken();
      if (Tok.is(TokenKind::kw_int))
        consumeToken();
      Kind = BuiltinKind::Short;
    } else if (Tok.is(TokenKind::kw_int)) {
      Kind = BuiltinKind::Int;
      consumeToken();
    } else if (Tok.is(TokenKind::kw_long)) {
      consumeToken();
      if (Tok.is(TokenKind::kw_long)) {
        consumeToken();
        Kind = BuiltinKind::LongLong;
      } else {
        Kind = BuiltinKind::Long;
      }
      if (Tok.is(TokenKind::kw_int))
        consumeToken();
    } else {
      // 'signed' alone means 'signed int'
      Kind = BuiltinKind::Int;
    }
    break;
    
  case TokenKind::kw_unsigned:
    consumeToken();
    if (Tok.is(TokenKind::kw_char)) {
      Kind = BuiltinKind::UnsignedChar;
      consumeToken();
    } else if (Tok.is(TokenKind::kw_short)) {
      consumeToken();
      if (Tok.is(TokenKind::kw_int))
        consumeToken();
      Kind = BuiltinKind::UnsignedShort;
    } else if (Tok.is(TokenKind::kw_int)) {
      Kind = BuiltinKind::UnsignedInt;
      consumeToken();
    } else if (Tok.is(TokenKind::kw_long)) {
      consumeToken();
      if (Tok.is(TokenKind::kw_long)) {
        consumeToken();
        Kind = BuiltinKind::UnsignedLongLong;
      } else {
        Kind = BuiltinKind::UnsignedLong;
      }
      if (Tok.is(TokenKind::kw_int))
        consumeToken();
    } else {
      // 'unsigned' alone means 'unsigned int'
      Kind = BuiltinKind::UnsignedInt;
    }
    break;
    
  default:
    // Not a builtin type
    return QualType();
  }
  
  // Create the builtin type
  BuiltinType *BT = Context.getBuiltinType(Kind);
  return QualType(BT, Qualifier::None);
}

/// parseDeclarator - Parse a declarator.
///
/// declarator ::= ptr-operator* direct-declarator
/// ptr-operator ::= '*' cvr-qualifiers?
///                | '&' 
///                | '&&'
///
QualType Parser::parseDeclarator(QualType Base) {
  if (Base.isNull())
    return Base;
  
  // Parse pointer operators
  while (true) {
    if (Tok.is(TokenKind::star)) {
      consumeToken();
      
      // Parse CVR qualifiers for pointer
      Qualifier Quals = Qualifier::None;
      while (true) {
        if (Tok.is(TokenKind::kw_const)) {
          Quals = Quals | Qualifier::Const;
          consumeToken();
        } else if (Tok.is(TokenKind::kw_volatile)) {
          Quals = Quals | Qualifier::Volatile;
          consumeToken();
        } else if (Tok.is(TokenKind::kw_restrict)) {
          Quals = Quals | Qualifier::Restrict;
          consumeToken();
        } else {
          break;
        }
      }
      
      // Create pointer type
      PointerType *PT = Context.getPointerType(Base.getTypePtr());
      Base = QualType(PT, Quals);
      
    } else if (Tok.is(TokenKind::amp)) {
      consumeToken();
      
      // Create lvalue reference type
      LValueReferenceType *RT = Context.getLValueReferenceType(Base.getTypePtr());
      Base = QualType(RT, Qualifier::None);
      
    } else if (Tok.is(TokenKind::ampamp)) {
      consumeToken();
      
      // Create rvalue reference type
      RValueReferenceType *RT = Context.getRValueReferenceType(Base.getTypePtr());
      Base = QualType(RT, Qualifier::None);
      
    } else {
      break;
    }
  }
  
  return Base;
}

/// parseArrayDimension - Parse an array dimension.
///
/// array-dimension ::= '[' expr? ']'
///
QualType Parser::parseArrayDimension(QualType Base) {
  if (Base.isNull())
    return Base;
  
  assert(Tok.is(TokenKind::l_square) && "Expected '['");
  consumeToken();
  
  // Parse size expression (optional)
  Expr *Size = nullptr;
  if (!Tok.is(TokenKind::r_square)) {
    Size = parseExpression();
  }
  
  // Expect ']'
  if (!Tok.is(TokenKind::r_square)) {
    emitError(DiagID::err_expected);
    return Base;
  }
  consumeToken();
  
  // Create array type
  ArrayType *AT = Context.getArrayType(Base.getTypePtr(), Size);
  return QualType(AT, Qualifier::None);
}

} // namespace blocktype
