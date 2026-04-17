//===--- TypeDeduction.cpp - Auto/Decltype Type Deduction --*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/TypeDeduction.h"
#include "blocktype/AST/ASTContext.h"

namespace blocktype {

using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// auto deduction
//===----------------------------------------------------------------------===//

QualType TypeDeduction::deduceAutoType(QualType DeclaredType, Expr *Init) {
  if (!Init) return QualType();

  QualType T = Init->getType();
  if (T.isNull()) return QualType();

  // 1. Strip top-level reference
  if (T->isReferenceType()) {
    auto *RefTy = static_cast<const ReferenceType *>(T.getTypePtr());
    T = QualType(RefTy->getReferencedType(), T.getQualifiers());
  }

  // 2. Strip top-level const/volatile (auto drops top-level CV)
  //    Unless declared as const auto
  if (!DeclaredType.isConstQualified()) {
    T = T.withoutConstQualifier().withoutVolatileQualifier();
  }

  // 3. Array decay to pointer
  if (T->isArrayType()) {
    const Type *ElemType = nullptr;
    if (T->getTypeClass() == TypeClass::ConstantArray) {
      auto *CA = static_cast<const ConstantArrayType *>(T.getTypePtr());
      ElemType = CA->getElementType();
    } else if (T->getTypeClass() == TypeClass::IncompleteArray) {
      auto *IA = static_cast<const IncompleteArrayType *>(T.getTypePtr());
      ElemType = IA->getElementType();
    }
    if (ElemType) {
      T = QualType(Context.getPointerType(ElemType), Qualifier::None);
    }
  }

  // 4. Function decay to function pointer
  if (T->isFunctionType()) {
    T = QualType(Context.getPointerType(T.getTypePtr()), Qualifier::None);
  }

  return T;
}

QualType TypeDeduction::deduceAutoRefType(QualType DeclaredType, Expr *Init) {
  if (!Init) return QualType();

  QualType T = Init->getType();
  if (T.isNull()) return QualType();

  // auto& binds to lvalues only; binding to rvalues is ill-formed
  // unless the reference is const-qualified (const auto&).
  if (Init->isRValue() && !DeclaredType.isConstQualified()) {
    if (Diags) {
      Diags->report(Init->getLocation(),
                    DiagID::err_non_const_lvalue_ref_binds_to_rvalue);
    }
    return QualType();
  }

  // auto& preserves the type including CV qualifiers
  return QualType(Context.getLValueReferenceType(T.getTypePtr()),
                  T.getQualifiers());
}

QualType TypeDeduction::deduceAutoForwardingRefType(Expr *Init) {
  if (!Init) return QualType();

  QualType T = Init->getType();
  if (T.isNull()) return QualType();

  // auto&& (forwarding reference / universal reference)
  // If init is lvalue → deduce as lvalue reference (T&)
  // If init is rvalue (xvalue or prvalue) → deduce as rvalue reference (T&&)
  if (Init->isLValue()) {
    return QualType(Context.getLValueReferenceType(T.getTypePtr()),
                    Qualifier::None);
  }
  return QualType(Context.getRValueReferenceType(T.getTypePtr()),
                  Qualifier::None);
}

QualType TypeDeduction::deduceAutoPointerType(Expr *Init) {
  if (!Init) return QualType();

  QualType T = Init->getType();
  if (T.isNull()) return QualType();

  // auto* x = &init; → deduce the pointee type
  if (T->isPointerType()) {
    auto *PtrTy = static_cast<const PointerType *>(T.getTypePtr());
    return QualType(PtrTy->getPointeeType(), T.getQualifiers());
  }

  // If init is not a pointer, error
  return QualType();
}

QualType TypeDeduction::deduceReturnType(Expr *ReturnExpr) {
  if (!ReturnExpr) return Context.getVoidType();

  QualType T = ReturnExpr->getType();
  if (T.isNull()) return QualType();

  // Strip reference for return type (auto return follows same rules as auto)
  return deduceAutoType(Context.getAutoType(), ReturnExpr);
}

QualType TypeDeduction::deduceFromInitList(llvm::ArrayRef<Expr *> Inits) {
  if (Inits.empty()) return QualType();

  // auto x = {1, 2, 3} → deduce from the first element
  // All elements must have the same type
  QualType FirstType = Inits[0]->getType();
  if (FirstType.isNull()) return QualType();

  // Verify all elements have the same type
  for (unsigned i = 1; i < Inits.size(); ++i) {
    QualType T = Inits[i]->getType();
    if (T.isNull() || T.getCanonicalType() != FirstType.getCanonicalType()) {
      // Inconsistent types in initializer list
      return QualType();
    }
  }

  return FirstType;
}

//===----------------------------------------------------------------------===//
// decltype deduction
//===----------------------------------------------------------------------===//

QualType TypeDeduction::deduceDecltypeType(Expr *E) {
  if (!E) return QualType();

  QualType T = E->getType();
  if (T.isNull()) return QualType();

  // decltype rules (C++ [dcl.type.decltype]):
  // - decltype(id) → declared type of id (no reference stripping)
  // - decltype(expr) → type of expr, preserving value category:
  //   - xvalue → T&&
  //   - lvalue → T&
  //   - prvalue → T
  if (E->isLValue()) {
    return QualType(Context.getLValueReferenceType(T.getTypePtr()),
                    T.getQualifiers());
  }
  if (E->isXValue()) {
    return QualType(Context.getRValueReferenceType(T.getTypePtr()),
                    T.getQualifiers());
  }
  // prvalue: return T as-is
  return T;
}

QualType TypeDeduction::deduceDecltypeAutoType(Expr *E) {
  if (!E) return QualType();

  // decltype(auto) follows decltype rules but for auto deduction context
  return deduceDecltypeType(E);
}

//===----------------------------------------------------------------------===//
// Template argument deduction (placeholder)
//===----------------------------------------------------------------------===//

bool TypeDeduction::deduceTemplateArguments(
    TemplateDecl *Template,
    llvm::ArrayRef<Expr *> Args,
    llvm::SmallVectorImpl<TemplateArgument> &DeducedArgs) {
  // TODO: Implement template argument deduction
  return false;
}

} // namespace blocktype
