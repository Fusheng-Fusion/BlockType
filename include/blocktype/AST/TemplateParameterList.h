//===--- TemplateParameterList.h - Template Parameter List -*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the TemplateParameterList class, which represents
// the list of template parameters in a template declaration.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

class Expr;
class NamedDecl;

/// TemplateParameterList - Represents the template parameter list in a
/// template declaration, including source location information.
///
/// Example:
///   template<typename T, typename U = vector<T>>
///   ^^^^^^^^ ^              ^              ^^^ ^
///   TemplateLoc              LAngleLoc      RAngleLoc
///
/// For partial specializations, this also holds the parameters:
///   template<typename T> class Vector<T*> { ... };
///            ^^^^^^^^^^
///            This TemplateParameterList holds T
class TemplateParameterList final {
  SourceLocation TemplateLoc;  // 'template' keyword location
  SourceLocation LAngleLoc;    // '<' location
  SourceLocation RAngleLoc;    // '>' location
  llvm::SmallVector<NamedDecl *, 8> Params;
  Expr *RequiresClause;        // C++20 requires-clause (optional)

public:
  TemplateParameterList(SourceLocation TemplateLoc, SourceLocation LAngleLoc,
                        SourceLocation RAngleLoc,
                        llvm::ArrayRef<NamedDecl *> Params,
                        Expr *RequiresClause = nullptr)
      : TemplateLoc(TemplateLoc), LAngleLoc(LAngleLoc),
        RAngleLoc(RAngleLoc), Params(Params.begin(), Params.end()),
        RequiresClause(RequiresClause) {}

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  /// Source location of the 'template' keyword.
  SourceLocation getTemplateLoc() const { return TemplateLoc; }

  /// Source location of the '<' (left angle bracket).
  SourceLocation getLAngleLoc() const { return LAngleLoc; }

  /// Source location of the '>' (right angle bracket).
  SourceLocation getRAngleLoc() const { return RAngleLoc; }

  /// Get the source range covering the entire template parameter list.
  SourceRange getSourceRange() const {
    return SourceRange(TemplateLoc, RAngleLoc);
  }

  //===--------------------------------------------------------------------===//
  // Parameter access
  //===--------------------------------------------------------------------===//

  /// Number of template parameters.
  unsigned size() const { return Params.size(); }

  /// Whether there are any template parameters.
  bool empty() const { return Params.empty(); }

  /// Get the Nth template parameter.
  NamedDecl *getParam(unsigned Idx) { return Params[Idx]; }
  const NamedDecl *getParam(unsigned Idx) const { return Params[Idx]; }

  /// Get all template parameters.
  llvm::ArrayRef<NamedDecl *> asArray() { return Params; }
  llvm::ArrayRef<const NamedDecl *> asArray() const {
    return {reinterpret_cast<const NamedDecl *const *>(Params.data()),
            Params.size()};
  }

  /// Get a mutable ArrayRef of parameters.
  llvm::ArrayRef<NamedDecl *> getParams() { return Params; }
  llvm::ArrayRef<NamedDecl *> getParams() const { return Params; }

  /// Add a parameter (used by legacy addTemplateParameter API).
  void addParam(NamedDecl *P) { Params.push_back(P); }

  //===--------------------------------------------------------------------===//
  // Iterators
  //===--------------------------------------------------------------------===//

  using iterator = llvm::SmallVector<NamedDecl *, 8>::iterator;
  using const_iterator = llvm::SmallVector<NamedDecl *, 8>::const_iterator;

  iterator begin() { return Params.begin(); }
  iterator end() { return Params.end(); }
  const_iterator begin() const { return Params.begin(); }
  const_iterator end() const { return Params.end(); }

  //===--------------------------------------------------------------------===//
  // Requires clause (C++20)
  //===--------------------------------------------------------------------===//

  /// Get the requires-clause expression, if any.
  Expr *getRequiresClause() const { return RequiresClause; }
  void setRequiresClause(Expr *E) { RequiresClause = E; }
  bool hasRequiresClause() const { return RequiresClause != nullptr; }

  //===--------------------------------------------------------------------===//
  // Utilities
  //===--------------------------------------------------------------------===//

  /// Check if this parameter list contains any parameter packs.
  bool hasParameterPack() const;

  /// Get the number of parameter packs in this list.
  unsigned getNumParameterPacks() const;

  /// Get the minimum number of template arguments required.
  /// This counts non-pack parameters without defaults.
  unsigned getMinRequiredArguments() const;

  /// Dump the template parameter list for debugging.
  void dump(llvm::raw_ostream &OS, unsigned Indent = 0) const;
};

} // namespace blocktype
