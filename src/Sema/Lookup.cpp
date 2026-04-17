//===--- Lookup.cpp - Name Lookup Implementation ----------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements name lookup: LookupResult, NestedNameSpecifier,
// Unqualified Lookup, Qualified Lookup, and ADL.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/Sema.h"
#include "blocktype/Sema/Lookup.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/ASTNode.h"
#include "blocktype/AST/DeclContext.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// LookupResult
//===----------------------------------------------------------------------===//

FunctionDecl *LookupResult::getAsFunction() const {
  if (Decls.empty()) return nullptr;
  if (Decls.size() > 1 && !Overloaded) return nullptr;

  for (auto *D : Decls) {
    if (auto *FD = dyn_cast<FunctionDecl>(static_cast<ASTNode *>(D)))
      return FD;
  }
  return nullptr;
}

TypeDecl *LookupResult::getAsTypeDecl() const {
  if (!isSingleResult()) return nullptr;
  return dyn_cast<TypeDecl>(static_cast<ASTNode *>(getFoundDecl()));
}

TagDecl *LookupResult::getAsTagDecl() const {
  if (!isSingleResult()) return nullptr;
  return dyn_cast<TagDecl>(static_cast<ASTNode *>(getFoundDecl()));
}

//===----------------------------------------------------------------------===//
// NestedNameSpecifier
//===----------------------------------------------------------------------===//

NestedNameSpecifier *NestedNameSpecifier::CreateGlobalSpecifier() {
  decltype(NestedNameSpecifier::Data) D;
  D.Namespace = nullptr;
  static NestedNameSpecifier Global(SpecifierKind::Global, D, nullptr);
  return &Global;
}

NestedNameSpecifier *NestedNameSpecifier::Create(ASTContext &Ctx,
                                                  NestedNameSpecifier *Prefix,
                                                  NamespaceDecl *NS) {
  void *Mem = Ctx.getAllocator().Allocate(sizeof(NestedNameSpecifier),
                                          alignof(NestedNameSpecifier));
  decltype(Data) D;
  D.Namespace = NS;
  return new (Mem) NestedNameSpecifier(Namespace, D, Prefix);
}

NestedNameSpecifier *NestedNameSpecifier::Create(ASTContext &Ctx,
                                                  NestedNameSpecifier *Prefix,
                                                  const blocktype::Type *T) {
  void *Mem = Ctx.getAllocator().Allocate(sizeof(NestedNameSpecifier),
                                          alignof(NestedNameSpecifier));
  decltype(Data) D;
  D.TypeSpec = T;
  return new (Mem) NestedNameSpecifier(TypeSpec, D, Prefix);
}

NestedNameSpecifier *NestedNameSpecifier::Create(ASTContext &Ctx,
                                                  NestedNameSpecifier *Prefix,
                                                  llvm::StringRef Identifier) {
  void *Mem = Ctx.getAllocator().Allocate(sizeof(NestedNameSpecifier),
                                          alignof(NestedNameSpecifier));
  decltype(Data) D;
  D.IdentifierStr = Identifier.data();
  return new (Mem) NestedNameSpecifier(SpecifierKind::Identifier, D, Prefix);
}

std::string NestedNameSpecifier::getAsString() const {
  std::string Result;
  if (Prefix) {
    Result += Prefix->getAsString();
  }

  switch (Kind) {
  case Global:
    Result += "::";
    break;
  case Namespace:
    Result += Data.Namespace->getName();
    Result += "::";
    break;
  case TypeSpec:
  case TemplateTypeSpec:
    Result += "<type>::";
    break;
  case Identifier:
    Result += Data.IdentifierStr;
    Result += "::";
    break;
  }

  return Result;
}

//===----------------------------------------------------------------------===//
// Unqualified Lookup [Task 4.3.2]
//===----------------------------------------------------------------------===//

LookupResult Sema::LookupUnqualifiedName(llvm::StringRef Name, Scope *S,
                                          LookupNameKind Kind) {
  LookupResult Result;

  // Walk up the scope chain
  for (Scope *Cur = S; Cur; Cur = Cur->getParent()) {
    if (NamedDecl *D = Cur->lookupInScope(Name)) {
      Result.addDecl(D);

      // Tag lookup: only tags, skip non-tags
      if (Kind == LookupNameKind::LookupTagName) {
        if (isa<TagDecl>(static_cast<ASTNode *>(D)))
          return Result;
        continue;
      }

      // Type name lookup: only type declarations
      if (Kind == LookupNameKind::LookupTypeName) {
        if (isa<TypeDecl>(static_cast<ASTNode *>(D))) {
          Result.setTypeName(true);
          return Result;
        }
        continue;
      }

      // Namespace name lookup
      if (Kind == LookupNameKind::LookupNamespaceName) {
        if (isa<NamespaceDecl>(static_cast<ASTNode *>(D)))
          return Result;
        continue;
      }

      // Non-function declaration: first match wins
      if (!isa<FunctionDecl>(static_cast<ASTNode *>(D)) &&
          !isa<CXXMethodDecl>(static_cast<ASTNode *>(D))) {
        return Result;
      }

      // Function: collect overloads from the same scope
      for (NamedDecl *ScopeD : Cur->decls()) {
        if (ScopeD == D) continue;
        if (ScopeD->getName() == Name) {
          if (isa<FunctionDecl>(static_cast<ASTNode *>(ScopeD)) ||
              isa<CXXMethodDecl>(static_cast<ASTNode *>(ScopeD))) {
            Result.addDecl(ScopeD);
          }
        }
      }

      if (Result.getNumDecls() > 1)
        Result.setOverloaded(true);

      return Result;
    }

    // Process using directives in this scope
    for (NamespaceDecl *NS : Cur->getUsingDirectives()) {
      if (NamedDecl *D = NS->lookup(Name)) {
        Result.addDecl(D);
        if (!isa<FunctionDecl>(static_cast<ASTNode *>(D)) &&
            !isa<CXXMethodDecl>(static_cast<ASTNode *>(D))) {
          return Result;
        }
      }
    }
  }

  // Fall back to global symbol table
  if (Result.empty()) {
    if (Kind == LookupNameKind::LookupTagName) {
      if (auto *TD = Symbols.lookupTag(Name))
        Result.addDecl(TD);
    } else if (Kind == LookupNameKind::LookupNamespaceName) {
      if (auto *ND = Symbols.lookupNamespace(Name))
        Result.addDecl(ND);
    } else if (Kind == LookupNameKind::LookupTypeName) {
      if (auto *TD = Symbols.lookupTypedef(Name)) {
        Result.addDecl(TD);
        Result.setTypeName(true);
      } else if (auto *TD = Symbols.lookupTag(Name)) {
        Result.addDecl(TD);
        Result.setTypeName(true);
      }
    } else {
      for (NamedDecl *D : Symbols.lookupOrdinary(Name)) {
        Result.addDecl(D);
      }
    }
  }

  if (Result.getNumDecls() > 1)
    Result.setOverloaded(true);

  return Result;
}

//===----------------------------------------------------------------------===//
// Qualified Lookup [Task 4.3.3]
//===----------------------------------------------------------------------===//

LookupResult Sema::LookupQualifiedName(llvm::StringRef Name,
                                         NestedNameSpecifier *NNS) {
  LookupResult Result;

  if (!NNS) return Result;

  DeclContext *DC = nullptr;

  switch (NNS->getKind()) {
  case NestedNameSpecifier::Global:
    if (CurTU)
      DC = CurTU->getDeclContext();
    break;

  case NestedNameSpecifier::Namespace:
    DC = NNS->getAsNamespace();
    break;

  case NestedNameSpecifier::TypeSpec: {
    const blocktype::Type *T = NNS->getAsType();
    if (T && T->isRecordType()) {
      auto *RT = static_cast<const RecordType *>(T);
      RecordDecl *RD = RT->getDecl();
      if (RD) {
        // Qualified lookup "Class::name" must search in the class itself.
        // Only CXXRecordDecl inherits DeclContext and supports member lookup.
        // Plain RecordDecl (C-style struct) does not inherit DeclContext.
        auto *CXXRD = dyn_cast<CXXRecordDecl>(static_cast<ASTNode *>(RD));
        if (CXXRD)
          DC = static_cast<DeclContext *>(CXXRD);
        // Non-CXX records cannot serve as DeclContext; qualified lookup fails.
      }
    }
    if (!DC && T && T->isEnumType()) {
      auto *ET = static_cast<const EnumType *>(T);
      EnumDecl *ED = ET->getDecl();
      if (ED)
        DC = static_cast<DeclContext *>(ED);
    }
    break;
  }

  case NestedNameSpecifier::TemplateTypeSpec:
    break;

  case NestedNameSpecifier::Identifier:
    break;
  }

  if (!DC) return Result;

  // Perform lookup in the resolved DeclContext
  if (NamedDecl *D = DC->lookup(Name)) {
    Result.addDecl(D);

    // Collect function overloads
    for (auto *ChildD : DC->decls()) {
      auto *ND = dyn_cast<NamedDecl>(static_cast<ASTNode *>(ChildD));
      if (ND && ND != D && ND->getName() == Name) {
        if (isa<FunctionDecl>(static_cast<ASTNode *>(ND)) ||
            isa<CXXMethodDecl>(static_cast<ASTNode *>(ND))) {
          Result.addDecl(ND);
        }
      }
    }
  }

  if (Result.getNumDecls() > 1)
    Result.setOverloaded(true);

  return Result;
}

//===----------------------------------------------------------------------===//
// ADL (Argument-Dependent Lookup) [Task 4.3.4]
//===----------------------------------------------------------------------===//

namespace {

/// Find the enclosing namespace DeclContext for a given DeclContext.
DeclContext *findEnclosingNamespace(DeclContext *DC) {
  if (!DC) return nullptr;
  DeclContext *Ctx = DC->getParent();
  while (Ctx) {
    if (Ctx->isNamespace())
      return Ctx;
    Ctx = Ctx->getParent();
  }
  return nullptr;
}

/// Find the NamespaceDecl* corresponding to a DeclContext that is a namespace.
/// This requires walking the parent context's declarations to find the
/// NamespaceDecl whose DeclContext matches.
NamespaceDecl *findNamespaceDecl(DeclContext *NSCtx) {
  if (!NSCtx || !NSCtx->isNamespace()) return nullptr;

  // Walk the parent's declarations to find the NamespaceDecl
  DeclContext *Parent = NSCtx->getParent();
  if (!Parent) return nullptr;

  for (auto *D : Parent->decls()) {
    auto *ND = dyn_cast<NamespaceDecl>(static_cast<ASTNode *>(D));
    if (ND && static_cast<DeclContext *>(ND) == NSCtx) {
      return ND;
    }
  }
  return nullptr;
}

} // anonymous namespace

void Sema::LookupADL(llvm::StringRef Name,
                      llvm::ArrayRef<Expr *> Args,
                      LookupResult &Result) {
  llvm::SmallPtrSet<NamespaceDecl *, 8> AssociatedNamespaces;
  llvm::SmallPtrSet<const RecordType *, 8> AssociatedClasses;

  for (Expr *Arg : Args) {
    CollectAssociatedNamespacesAndClasses(Arg->getType(),
                                           AssociatedNamespaces,
                                           AssociatedClasses);
  }

  // Look up the name in each associated namespace
  for (NamespaceDecl *NS : AssociatedNamespaces) {
    if (NamedDecl *D = NS->lookup(Name)) {
      Result.addDecl(D);
    }
  }

  // Look up friend functions in associated classes
  for (const RecordType *RT : AssociatedClasses) {
    RecordDecl *RD = RT->getDecl();
    auto *CXXRD = dyn_cast<CXXRecordDecl>(static_cast<ASTNode *>(RD));
    if (!CXXRD) continue;

    DeclContext *DC = CXXRD->getDeclContext();
    for (auto *D : DC->decls()) {
      auto *ND = dyn_cast<NamedDecl>(static_cast<ASTNode *>(D));
      if (!ND || ND->getName() != Name) continue;
      if (isa<FunctionDecl>(static_cast<ASTNode *>(ND))) {
        Result.addDecl(ND);
      }
    }
  }

  if (Result.getNumDecls() > 1)
    Result.setOverloaded(true);
}

void Sema::CollectAssociatedNamespacesAndClasses(
    QualType T,
    llvm::SmallPtrSetImpl<NamespaceDecl *> &Namespaces,
    llvm::SmallPtrSetImpl<const RecordType *> &Classes) {
  if (T.isNull()) return;

  const Type *Ty = T.getTypePtr();

  // Class type: add class + enclosing namespace
  if (Ty->isRecordType()) {
    auto *RT = static_cast<const RecordType *>(Ty);
    Classes.insert(RT);

    RecordDecl *RD = RT->getDecl();
    auto *CXXRD = dyn_cast<CXXRecordDecl>(static_cast<ASTNode *>(RD));
    if (CXXRD) {
      DeclContext *NSCtx = findEnclosingNamespace(CXXRD->getDeclContext());
      if (NSCtx) {
        NamespaceDecl *NS = findNamespaceDecl(NSCtx);
        if (NS)
          Namespaces.insert(NS);
      }

      // Process base classes for ADL
      for (const auto &Base : CXXRD->bases()) {
        QualType BaseType = Base.getType();
        if (BaseType.getTypePtr() && BaseType->isRecordType()) {
          auto *BaseRT = static_cast<const RecordType *>(BaseType.getTypePtr());
          Classes.insert(BaseRT);
          RecordDecl *BaseRD = BaseRT->getDecl();
          auto *BaseCXXRD = dyn_cast<CXXRecordDecl>(
              static_cast<ASTNode *>(BaseRD));
          if (BaseCXXRD) {
            DeclContext *BaseNS = findEnclosingNamespace(
                BaseCXXRD->getDeclContext());
            if (BaseNS) {
              NamespaceDecl *NS = findNamespaceDecl(BaseNS);
              if (NS)
                Namespaces.insert(NS);
            }
          }
        }
      }
    }
  }

  // Enum type: add enclosing namespace
  if (Ty->isEnumType()) {
    auto *ET = static_cast<const EnumType *>(Ty);
    EnumDecl *ED = ET->getDecl();
    if (ED) {
      DeclContext *EnumDC = static_cast<DeclContext *>(ED);
      DeclContext *NSCtx = findEnclosingNamespace(EnumDC);
      if (NSCtx) {
        NamespaceDecl *NS = findNamespaceDecl(NSCtx);
        if (NS)
          Namespaces.insert(NS);
      }
    }
  }

  // Pointer type: recurse into pointee
  if (Ty->isPointerType()) {
    auto *PT = static_cast<const PointerType *>(Ty);
    CollectAssociatedNamespacesAndClasses(
        QualType(PT->getPointeeType(), Qualifier::None),
        Namespaces, Classes);
  }

  // Reference type: recurse into referenced type
  if (Ty->isReferenceType()) {
    auto *RT = static_cast<const ReferenceType *>(Ty);
    CollectAssociatedNamespacesAndClasses(
        QualType(RT->getReferencedType(), Qualifier::None),
        Namespaces, Classes);
  }
}
