//===--- TypeDeduction.h - Auto/Decltype Type Deduction -----*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the TypeDeduction class for auto and decltype type
// deduction.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/Type.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/ASTContext.h"
#include "llvm/ADT/SmallVector.h"

namespace blocktype {

/// TypeDeduction - Handles auto and decltype type deduction.
///
/// Follows the C++ standard rules:
/// - auto deduction: [dcl.spec.auto]
/// - decltype deduction: [dcl.type.decltype]
/// - decltype(auto) deduction: [dcl.spec.auto]
class TypeDeduction {
  ASTContext &Context;

public:
  explicit TypeDeduction(ASTContext &C) : Context(C) {}

  //===------------------------------------------------------------------===//
  // auto deduction
  //===------------------------------------------------------------------===//

  /// Deduce the type for `auto x = init;`.
  /// Rules (C++ [dcl.spec.auto]):
  /// 1. Strip top-level reference from init type
  /// 2. Strip top-level const/volatile (unless declared as `const auto`)
  /// 3. Array decays to pointer
  /// 4. Function decays to function pointer
  QualType deduceAutoType(QualType DeclaredType, Expr *Init);

  /// Deduce the type for `auto& x = init;`.
  QualType deduceAutoRefType(QualType DeclaredType, Expr *Init);

  /// Deduce the type for `auto&& x = init;` (forwarding reference).
  QualType deduceAutoForwardingRefType(Expr *Init);

  /// Deduce the type for `auto* x = &init;`.
  QualType deduceAutoPointerType(Expr *Init);

  /// Deduce return type for `auto f() { return expr; }`.
  QualType deduceReturnType(Expr *ReturnExpr);

  /// Deduce from initializer list `auto x = {1, 2, 3}`.
  QualType deduceFromInitList(llvm::ArrayRef<Expr *> Inits);

  //===------------------------------------------------------------------===//
  // decltype deduction
  //===------------------------------------------------------------------===//

  /// Deduce the type for `decltype(expr)`.
  /// Rules (C++ [dcl.type.decltype]):
  /// - decltype(id) → declared type of id
  /// - decltype(expr) → type of expr, preserving value category
  /// - decltype((id)) → reference to declared type of id
  QualType deduceDecltypeType(Expr *E);

  /// Deduce the type for `decltype(auto)`.
  QualType deduceDecltypeAutoType(Expr *E);

  //===------------------------------------------------------------------===//
  // Template argument deduction (placeholder for later)
  //===------------------------------------------------------------------===//

  /// Deduce template arguments from a function call.
  /// Returns true if deduction succeeded.
  bool deduceTemplateArguments(TemplateDecl *Template,
                                llvm::ArrayRef<Expr *> Args,
                                llvm::SmallVectorImpl<TemplateArgument> &DeducedArgs);
};

} // namespace blocktype
