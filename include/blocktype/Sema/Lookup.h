//===--- Lookup.h - Name Lookup Types -----------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines types used for name lookup: LookupNameKind, LookupResult,
// and NestedNameSpecifier.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/Decl.h"
#include "blocktype/Sema/Scope.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"

namespace blocktype {

/// LookupNameKind - The kind of name lookup being performed.
///
/// Different lookup kinds follow different rules, e.g., tag lookup
/// and ordinary name lookup can return different results for the
/// same name (C++ allows `int X; class X;` in the same scope).
enum class LookupNameKind {
  /// Ordinary name lookup: variables, functions, enumerators, etc.
  LookupOrdinaryName,

  /// Tag name lookup: class, struct, union, enum.
  LookupTagName,

  /// Member name lookup: class/struct members.
  LookupMemberName,

  /// Operator name lookup: operator overloading.
  LookupOperatorName,

  /// Namespace name lookup.
  LookupNamespaceName,

  /// Type name lookup: any type (class, enum, typedef, template).
  LookupTypeName,

  /// Concept name lookup (C++20).
  LookupConceptName,
};

/// LookupResult - Result of a name lookup operation.
///
/// Can contain:
/// - No results (name not found)
/// - A single result (unique declaration)
/// - Multiple results (overloaded functions)
/// - Ambiguous results (error)
class LookupResult {
  llvm::SmallVector<NamedDecl *, 4> Decls;
  llvm::SmallPtrSet<NamedDecl *, 4> FoundDecls; // For deduplication
  bool Ambiguous = false;
  bool TypeName = false;       // Found result is a type name
  bool Overloaded = false;     // Found multiple function declarations
  bool Dependent = false;      // Lookup occurred in a dependent (template) context

public:
  LookupResult() = default;

  /// Construct with a single result.
  explicit LookupResult(NamedDecl *D) { Decls.push_back(D); }

  //===------------------------------------------------------------------===//
  // Adding results
  //===------------------------------------------------------------------===//

  /// Add a declaration to the result set (with deduplication).
  void addDecl(NamedDecl *D) {
    if (FoundDecls.insert(D).second)
      Decls.push_back(D);
  }

  /// Add all declarations from another result (with deduplication).
  void addAllDecls(const LookupResult &Other) {
    for (auto *D : Other.Decls)
      addDecl(D);
  }

  /// Mark as ambiguous.
  void setAmbiguous(bool A = true) { Ambiguous = A; }

  /// Mark that the result is a type name.
  void setTypeName(bool T = true) { TypeName = T; }

  /// Mark that the result is overloaded.
  void setOverloaded(bool O = true) { Overloaded = O; }

  /// Mark that the lookup happened in a dependent (template) context.
  void setDependent(bool D = true) { Dependent = D; }

  //===------------------------------------------------------------------===//
  // Result queries
  //===------------------------------------------------------------------===//

  /// Is the result empty (no declarations found)?
  bool empty() const { return Decls.empty(); }

  /// Is there exactly one result?
  bool isSingleResult() const { return Decls.size() == 1; }

  /// Is the result ambiguous?
  bool isAmbiguous() const { return Ambiguous; }

  /// Is this an overloaded result (multiple functions)?
  bool isOverloaded() const { return Overloaded; }

  /// Was this lookup performed in a dependent (template) context?
  /// When true and result is empty, the name may resolve at instantiation time.
  bool isDependent() const { return Dependent; }

  /// Is the result a type name?
  bool isTypeName() const { return TypeName; }

  //===------------------------------------------------------------------===//
  // Result access
  //===------------------------------------------------------------------===//

  /// Get the first (or only) declaration.
  NamedDecl *getFoundDecl() const {
    return Decls.empty() ? nullptr : Decls.front();
  }

  /// Get all declarations.
  llvm::ArrayRef<NamedDecl *> getDecls() const { return Decls; }

  /// Get the number of declarations.
  unsigned getNumDecls() const { return Decls.size(); }

  /// Get declaration at index.
  NamedDecl *operator[](unsigned I) const { return Decls[I]; }

  /// Resolve to a single function declaration (for non-overloaded calls).
  /// Returns nullptr if ambiguous or not found.
  FunctionDecl *getAsFunction() const;

  /// Resolve to a single type declaration.
  TypeDecl *getAsTypeDecl() const;

  /// Resolve to a single tag declaration.
  TagDecl *getAsTagDecl() const;

  /// Clear all results and reset all flags.
  void clear() {
    Decls.clear();
    FoundDecls.clear();
    Ambiguous = false;
    TypeName = false;
    Overloaded = false;
    Dependent = false;
  }
};

/// NestedNameSpecifier - Represents a C++ nested-name-specifier.
///
/// Example: For `A::B::C`, the NestedNameSpecifier is A::B::
/// Each component is either a namespace, a type, or the global specifier.
class NestedNameSpecifier {
public:
  enum SpecifierKind {
    /// Global scope: ::
    Global,

    /// Namespace: Ns::
    Namespace,

    /// Type (class/struct/enum/typedef): Type::
    TypeSpec,

    /// Template type: Template<T>::
    TemplateTypeSpec,

    /// Identifier (unresolved): Ident::
    Identifier,
  };

private:
  SpecifierKind Kind;
  union {
    NamespaceDecl *Namespace;
    const blocktype::Type *TypeSpec;
    TemplateDecl *Template;
    const char *IdentifierStr; // Stored as const char* for trivial construction
  } Data;
  NestedNameSpecifier *Prefix;

  // Private constructor for in-place initialization
  NestedNameSpecifier(SpecifierKind K, decltype(Data) D, NestedNameSpecifier *P)
      : Kind(K), Data(D), Prefix(P) {}

public:
  // Static factory methods
  static NestedNameSpecifier *CreateGlobalSpecifier();
  static NestedNameSpecifier *Create(ASTContext &Ctx,
                                      NestedNameSpecifier *Prefix,
                                      NamespaceDecl *NS);
  static NestedNameSpecifier *Create(ASTContext &Ctx,
                                      NestedNameSpecifier *Prefix,
                                      const blocktype::Type *T);
  static NestedNameSpecifier *Create(ASTContext &Ctx,
                                      NestedNameSpecifier *Prefix,
                                      llvm::StringRef Identifier);

  // Accessors
  SpecifierKind getKind() const { return Kind; }
  NestedNameSpecifier *getPrefix() const { return Prefix; }

  NamespaceDecl *getAsNamespace() const {
    return Kind == Namespace ? Data.Namespace : nullptr;
  }
  const blocktype::Type *getAsType() const {
    return Kind == TypeSpec || Kind == TemplateTypeSpec ? Data.TypeSpec : nullptr;
  }
  TemplateDecl *getAsTemplate() const {
    return Kind == TemplateTypeSpec ? Data.Template : nullptr;
  }
  llvm::StringRef getAsIdentifier() const {
    return Kind == Identifier ? llvm::StringRef(Data.IdentifierStr)
                               : llvm::StringRef();
  }

  /// Convert to string representation (for diagnostics).
  std::string getAsString() const;
};

} // namespace blocktype
