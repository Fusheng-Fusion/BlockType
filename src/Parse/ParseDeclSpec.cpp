//===--- ParseDeclSpec.cpp - Declaration Specifier Parsing ---*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements parseDeclSpecifierSeq() and related functions.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Parse/Parser.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Lex/Token.h"
#include "llvm/ADT/SmallVector.h"

namespace blocktype {

//===----------------------------------------------------------------------===//
// parseDeclSpecifierSeq - Parse declaration specifiers into a DeclSpec.
//===----------------------------------------------------------------------===//
//
// decl-specifier-seq ::= decl-specifier+
// decl-specifier ::= storage-class-specifier
//                  | function-specifier
//                  | friend | constexpr | consteval | constinit | typedef
//                  | type-specifier
//
void Parser::parseDeclSpecifierSeq(DeclSpec &DS) {
  bool SeenType = false;

  while (true) {
    // 1. Storage class specifiers
    if (Tok.is(TokenKind::kw_static)) {
      if (DS.SC != StorageClass::None)
        emitError(DiagID::err_expected);
      DS.SC = StorageClass::Static;
      DS.SCLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_extern)) {
      if (DS.SC != StorageClass::None)
        emitError(DiagID::err_expected);
      DS.SC = StorageClass::Extern;
      DS.SCLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_mutable)) {
      if (DS.SC != StorageClass::None)
        emitError(DiagID::err_expected);
      DS.SC = StorageClass::Mutable;
      DS.SCLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_thread_local)) {
      if (DS.SC != StorageClass::None)
        emitError(DiagID::err_expected);
      DS.SC = StorageClass::ThreadLocal;
      DS.SCLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_register)) {
      if (DS.SC != StorageClass::None)
        emitError(DiagID::err_expected);
      DS.SC = StorageClass::Register;
      DS.SCLoc = Tok.getLocation();
      consumeToken();
      continue;
    }

    // 2. Function specifiers
    if (Tok.is(TokenKind::kw_inline)) {
      DS.IsInline = true;
      DS.InlineLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_virtual)) {
      DS.IsVirtual = true;
      DS.VirtualLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_explicit)) {
      DS.IsExplicit = true;
      DS.ExplicitLoc = Tok.getLocation();
      consumeToken();
      continue;
    }

    // 3. Other specifiers
    if (Tok.is(TokenKind::kw_friend)) {
      DS.IsFriend = true;
      DS.FriendLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_constexpr)) {
      DS.IsConstexpr = true;
      DS.ConstexprLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_consteval)) {
      DS.IsConsteval = true;
      DS.ConstevalLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_constinit)) {
      DS.IsConstinit = true;
      DS.ConstinitLoc = Tok.getLocation();
      consumeToken();
      continue;
    }
    if (Tok.is(TokenKind::kw_typedef)) {
      DS.IsTypedef = true;
      DS.TypedefLoc = Tok.getLocation();
      consumeToken();
      continue;
    }

    // 4. Type specifier (only one allowed)
    if (!SeenType) {
      QualType Ty = parseTypeSpecifier();
      if (!Ty.isNull()) {
        DS.Type = Ty;
        DS.TypeLoc = Tok.getLocation();
        SeenType = true;
        continue;
      }
    }

    // Nothing matched, stop
    break;
  }
}

} // namespace blocktype
