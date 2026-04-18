//===--- DeclSpec.h - Declaration Specifier ------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the DeclSpec structure for parsing declarations.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceLocation.h"

namespace blocktype {

/// StorageClass - Storage class specifiers.
enum class StorageClass {
  None,
  Static,
  Extern,
  Mutable,
  ThreadLocal,
  Register,
};

/// DeclSpec - Captures all declaration specifiers parsed before the declarator.
///
/// This includes: type specifier, storage class, function specifiers,
/// and other specifiers like friend/constexpr.
struct DeclSpec {
  //===--------------------------------------------------------------------===//
  // Type specifier
  //===--------------------------------------------------------------------===//
  QualType Type;
  SourceLocation TypeLoc;

  //===--------------------------------------------------------------------===//
  // Storage class
  //===--------------------------------------------------------------------===//
  StorageClass SC = StorageClass::None;
  SourceLocation SCLoc;

  //===--------------------------------------------------------------------===//
  // Function specifiers
  //===--------------------------------------------------------------------===//
  bool IsInline : 1 = false;
  bool IsVirtual : 1 = false;
  bool IsExplicit : 1 = false;
  SourceLocation InlineLoc;
  SourceLocation VirtualLoc;
  SourceLocation ExplicitLoc;

  //===--------------------------------------------------------------------===//
  // Other specifiers
  //===--------------------------------------------------------------------===//
  bool IsFriend : 1 = false;
  bool IsConstexpr : 1 = false;
  bool IsConsteval : 1 = false;
  bool IsConstinit : 1 = false;
  SourceLocation FriendLoc;
  SourceLocation ConstexprLoc;
  SourceLocation ConstevalLoc;
  SourceLocation ConstinitLoc;

  //===--------------------------------------------------------------------===//
  // Typedef specifier
  //===--------------------------------------------------------------------===//
  bool IsTypedef : 1 = false;
  SourceLocation TypedefLoc;

  //===--------------------------------------------------------------------===//
  // C++11 attributes
  //===--------------------------------------------------------------------===//
  class AttributeListDecl *AttrList = nullptr;

  //===--------------------------------------------------------------------===//
  // Query methods
  //===--------------------------------------------------------------------===//

  bool hasTypeSpecifier() const { return !Type.isNull(); }

  bool hasStorageClass() const { return SC != StorageClass::None; }

  bool isFunctionSpecifier() const {
    return IsInline || IsVirtual || IsExplicit;
  }

  /// Returns true if any specifier has been set (including type).
  bool hasAnySpecifier() const {
    return hasTypeSpecifier() || hasStorageClass() || IsInline || IsVirtual ||
           IsExplicit || IsFriend || IsConstexpr || IsConsteval || IsConstinit ||
           IsTypedef;
  }
};

} // namespace blocktype
