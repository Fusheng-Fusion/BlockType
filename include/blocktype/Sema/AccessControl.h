//===--- AccessControl.h - C++ Access Control ---------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the AccessControl class for C++ access control checking.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/Decl.h"
#include "blocktype/AST/DeclContext.h"
#include "blocktype/Basic/SourceLocation.h"

namespace blocktype {

/// AccessControl - Performs C++ access control checking.
///
/// Access levels (defined in Decl.h):
/// - AS_public: accessible from anywhere
/// - AS_protected: accessible from the class and its derived classes
/// - AS_private: accessible only from the class itself and friends
///
/// The AccessSpecifier enum is already defined in Decl.h.
class AccessControl {
public:
  /// Check if `Member` is accessible from `AccessingContext`.
  ///
  /// \param Member The member being accessed.
  /// \param MemberAccess The declared access level of the member.
  /// \param AccessingContext The context from which access is attempted.
  /// \param ClassContext The class that declares the member.
  /// \return true if access is allowed, false otherwise.
  static bool isAccessible(NamedDecl *Member,
                            AccessSpecifier MemberAccess,
                            DeclContext *AccessingContext,
                            CXXRecordDecl *ClassContext);

  /// Check access for a member access expression (obj.member or ptr->member).
  static bool CheckMemberAccess(NamedDecl *Member,
                                 AccessSpecifier Access,
                                 CXXRecordDecl *MemberClass,
                                 DeclContext *AccessingContext,
                                 SourceLocation AccessLoc);

  /// Check access for a base class specifier.
  static bool CheckBaseClassAccess(CXXRecordDecl *Base,
                                    AccessSpecifier Access,
                                    CXXRecordDecl *Derived,
                                    SourceLocation AccessLoc);

  /// Check access for a friend declaration.
  static bool CheckFriendAccess(NamedDecl *Friend,
                                 CXXRecordDecl *Class,
                                 DeclContext *AccessingContext);

  /// Get the effective access of a member.
  static AccessSpecifier getEffectiveAccess(NamedDecl *D);

  /// Check if DerivedClass is derived from BaseClass.
  static bool isDerivedFrom(CXXRecordDecl *Derived, CXXRecordDecl *Base);
};

} // namespace blocktype
