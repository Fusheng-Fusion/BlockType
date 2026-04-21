//===--- TemplateInstantiator.cpp - Template Instantiation Engine -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/TemplateInstantiation.h"
#include "blocktype/Sema/Sema.h"
#include "blocktype/Sema/SFINAE.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/TemplateParameterList.h"
#include "blocktype/Basic/DiagnosticIDs.h"
#include "llvm/Support/Casting.h"
#include <string>

namespace blocktype {

//===----------------------------------------------------------------------===//
// TemplateInstantiator Implementation
//===----------------------------------------------------------------------===//

TemplateInstantiator::TemplateInstantiator(Sema &Sema)
    : SemaRef(Sema) {}

FunctionDecl *TemplateInstantiator::InstantiateFunctionTemplate(
    FunctionTemplateDecl *FuncTemplate,
    llvm::ArrayRef<TemplateArgument> TemplateArgs) {
  
  if (FuncTemplate == nullptr) {
    return nullptr;
  }
  
  // Step 1: Check if a specialization already exists (cache lookup)
  if (auto *Existing = FuncTemplate->findSpecialization(TemplateArgs)) {
    return Existing;
  }
  
  // Step 2: Enter SFINAE context for substitution
  unsigned SavedErrors = SemaRef.getDiagnostics().getNumErrors();
  SFINAEGuard SFINAEGuard(SFContext, SavedErrors, &SemaRef.getDiagnostics());
  
  // Step 3: Delegate to Sema for actual instantiation
  // The SFINAE context will catch any substitution failures
  FunctionDecl *Result = SemaRef.InstantiateFunctionTemplate(
      FuncTemplate, TemplateArgs, FuncTemplate->getLocation());
  
  // Step 4: If substitution failed during instantiation, return nullptr
  // (the candidate is removed from overload set per SFINAE rules)
  if (Result == nullptr || SemaRef.getDiagnostics().hasErrorOccurred()) {
    return nullptr;
  }
  
  return Result;
}

CXXRecordDecl *TemplateInstantiator::InstantiateClassTemplate(
    ClassTemplateDecl *ClassTemplate,
    llvm::ArrayRef<TemplateArgument> TemplateArgs) {
  
  if (ClassTemplate == nullptr) {
    return nullptr;
  }
  
  // Check if a specialization already exists
  if (auto *Existing = ClassTemplate->findSpecialization(TemplateArgs)) {
    return Existing;
  }
  
  // TODO: Implement full class template instantiation
  // For now, return the templated declaration as a placeholder
  return llvm::dyn_cast<CXXRecordDecl>(ClassTemplate->getTemplatedDecl());
}

Expr *TemplateInstantiator::InstantiateFoldExpr(
    CXXFoldExpr *FoldExpr,
    llvm::ArrayRef<TemplateArgument> PackArgs) {
  
  if (FoldExpr == nullptr) {
    return nullptr;
  }
  
  // Get the pack expansion arguments
  llvm::SmallVector<TemplateArgument, 4> PackElements;
  for (const auto &Arg : PackArgs) {
    if (Arg.isPack()) {
      for (const auto &Elem : Arg.getAsPack()) {
        PackElements.push_back(Elem);
      }
    }
  }
  
  // If pack is empty, return the init value or pattern
  // For empty packs, fold expressions return their identity element
  // which is stored in LHS (for left fold) or RHS (for right fold)
  if (PackElements.empty()) {
    // Return LHS for left fold, RHS for right fold, or pattern as fallback
    if (FoldExpr->getLHS() != nullptr) {
      return FoldExpr->getLHS();
    }
    if (FoldExpr->getRHS() != nullptr) {
      return FoldExpr->getRHS();
    }
    return FoldExpr->getPattern();
  }
  
  // TODO: Implement full fold expression instantiation
  // For now, return the pattern as a placeholder
  return FoldExpr->getPattern();
}

Expr *TemplateInstantiator::InstantiatePackIndexingExpr(
    PackIndexingExpr *PIE,
    llvm::ArrayRef<TemplateArgument> PackArgs) {
  
  if (PIE == nullptr) {
    return nullptr;
  }
  
  // Get the pack expansion arguments
  llvm::SmallVector<TemplateArgument, 4> PackElements;
  for (const auto &Arg : PackArgs) {
    if (Arg.isPack()) {
      for (const auto &Elem : Arg.getAsPack()) {
        PackElements.push_back(Elem);
      }
    }
  }
  
  // Get the index value
  Expr *IndexExpr = PIE->getIndex();
  uint64_t Index = 0;
  
  // Try to evaluate the index as a constant
  if (auto *IntLit = llvm::dyn_cast<IntegerLiteral>(IndexExpr)) {
    Index = IntLit->getValue().getZExtValue();
  } else {
    // Runtime index - cannot instantiate at compile time
    // Return the original expression for runtime evaluation
    return PIE;
  }
  
  // Check bounds
  if (PackElements.empty()) {
    // Empty pack - cannot index
    SemaRef.Diag(PIE->getLocation(), DiagID::err_pack_index_empty_pack);
    return nullptr;
  }
  
  if (Index >= PackElements.size()) {
    // Out of bounds - emit diagnostic
    SemaRef.Diag(IndexExpr->getLocation(), DiagID::err_pack_index_out_of_bounds,
                 std::to_string(Index) + " " + std::to_string(PackElements.size()));
    return nullptr;
  }
  
  // Build substituted expressions from pack elements
  llvm::SmallVector<Expr *, 4> SubstitutedExprs;
  ASTContext &Context = SemaRef.getASTContext();
  
  for (const auto &Elem : PackElements) {
    if (Elem.isIntegral()) {
      // Create an integer literal for integral arguments
      auto *IntLit = Context.create<IntegerLiteral>(SourceLocation(1),
                                                     Elem.getAsIntegral(),
                                                     Context.getIntType());
      SubstitutedExprs.push_back(IntLit);
    } else if (Elem.isType()) {
      // Type template arguments are primarily used in type contexts (e.g., using T = Ts...[0])
      // In expression context, we create a placeholder for now.
      // A full implementation would need a TypeExpr or similar AST node.
      // For now, we skip type arguments in expression context.
      // TODO: Add TypeExpr support for type template arguments in expression context
      continue;
    } else if (Elem.isExpression()) {
      // Use the expression directly
      if (Expr *ExprVal = Elem.getAsExpr()) {
        SubstitutedExprs.push_back(ExprVal);
      }
    } else if (Elem.isDeclaration()) {
      // Create a declaration reference for declaration arguments
      if (auto *Decl = Elem.getAsDecl()) {
        // DeclRefExpr expects (SourceLocation, ValueDecl*, StringRef)
        // ValueDecl is the base class for variables, functions, etc.
        if (auto *ValueDecl = llvm::dyn_cast<ValueDecl>(Decl)) {
          auto *DeclRef = Context.create<DeclRefExpr>(SourceLocation(1), ValueDecl, ValueDecl->getName());
          SubstitutedExprs.push_back(DeclRef);
        }
      }
    }
    // Note: Template template arguments are not directly representable as expressions
    // They would need special handling in a full implementation
  }
  
  // Set the substituted expressions on the PackIndexingExpr
  PIE->setSubstitutedExprs(SubstitutedExprs);
  
  // Return the Nth element if we have substituted expressions
  if (!SubstitutedExprs.empty() && Index < SubstitutedExprs.size()) {
    return SubstitutedExprs[Index];
  }
  
  // Return the original expression if we couldn't substitute
  return PIE;
}

llvm::SmallVector<Expr *, 4>
TemplateInstantiator::ExpandPack(Expr *Pattern,
                                 llvm::ArrayRef<TemplateArgument> PackArgs) {
  
  llvm::SmallVector<Expr *, 4> Result;
  
  if (Pattern == nullptr) {
    return Result;
  }
  
  // Get the pack expansion arguments
  llvm::SmallVector<TemplateArgument, 4> PackElements;
  for (const auto &Arg : PackArgs) {
    if (Arg.isPack()) {
      for (const auto &Elem : Arg.getAsPack()) {
        PackElements.push_back(Elem);
      }
    }
  }
  
  // Expand the pattern for each pack element
  ASTContext &Context = SemaRef.getASTContext();
  for (size_t Idx = 0; Idx < PackElements.size(); ++Idx) {
    // TODO: Implement proper pattern substitution
    // For now, clone the pattern for each element
    if (auto *IntLit = llvm::dyn_cast<IntegerLiteral>(Pattern)) {
      auto *NewIntLit = Context.create<IntegerLiteral>(IntLit->getLocation(),
                                                        IntLit->getValue(),
                                                        IntLit->getType());
      Result.push_back(NewIntLit);
    } else {
      // For other expressions, just add the pattern
      Result.push_back(Pattern);
    }
  }
  
  return Result;
}

} // namespace blocktype
