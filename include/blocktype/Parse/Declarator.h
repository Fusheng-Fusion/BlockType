//===--- Declarator.h - Declarator and DeclarationName ------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines DeclarationName, Declarator, and DeclaratorContext
// for structured declarator parsing.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/Parse/DeclSpec.h"
#include "blocktype/Parse/DeclaratorChunk.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace blocktype {

class ASTContext;

/// DeclaratorContext - Where the declarator appears.
enum class DeclaratorContext {
  FileContext,           // File-level declaration
  BlockContext,          // Block-level (inside function body)
  MemberContext,         // Class/struct member
  ParameterContext,      // Function parameter
  TemplateParamContext,  // Non-type template parameter
  ConditionContext,      // Condition (if/while/for/switch)
  TypeNameContext,       // Pure type name (no declarator name)
  CXXNewContext,         // new-type-id
};

//===----------------------------------------------------------------------===//
// DeclarationName
//===----------------------------------------------------------------------===//

/// DeclarationName - Represents the name of a declaration.
///
/// Supports: identifiers, constructor/destructor names, operator names,
/// conversion function names, etc.
class DeclarationName {
public:
  enum class NameKind {
    Identifier,                // Regular identifier: x, foo
    CXXConstructorName,        // Constructor: ClassName()
    CXXDestructorName,         // Destructor: ~ClassName()
    CXXConversionFunctionName, // operator type()
    CXXOperatorName,           // operator+, operator<<, etc.
    CXXLiteralOperatorName,    // operator""_s
    CXXDeductionGuideName,     // Class template deduction guide
  };

private:
  NameKind Kind = NameKind::Identifier;
  llvm::StringRef Name; // Identifier text (for Identifier, LiteralOperator)
  QualType NamedType;   // Type for constructor/destructor/conversion

public:
  DeclarationName() = default;
  explicit DeclarationName(llvm::StringRef N) : Name(N) {}

  static DeclarationName getConstructor(QualType T) {
    DeclarationName DN;
    DN.Kind = NameKind::CXXConstructorName;
    DN.NamedType = T;
    return DN;
  }

  static DeclarationName getDestructor(QualType T) {
    DeclarationName DN;
    DN.Kind = NameKind::CXXDestructorName;
    DN.NamedType = T;
    return DN;
  }

  static DeclarationName getConversionFunction(QualType T) {
    DeclarationName DN;
    DN.Kind = NameKind::CXXConversionFunctionName;
    DN.NamedType = T;
    return DN;
  }

  static DeclarationName getOperatorName(llvm::StringRef Op) {
    DeclarationName DN;
    DN.Kind = NameKind::CXXOperatorName;
    DN.Name = Op;
    return DN;
  }

  // Accessors
  NameKind getKind() const { return Kind; }
  bool isIdentifier() const { return Kind == NameKind::Identifier; }
  bool isConstructorName() const { return Kind == NameKind::CXXConstructorName; }
  bool isDestructorName() const { return Kind == NameKind::CXXDestructorName; }

  llvm::StringRef getIdentifier() const {
    assert(isIdentifier());
    return Name;
  }

  QualType getNamedType() const {
    assert(Kind == NameKind::CXXConstructorName ||
           Kind == NameKind::CXXDestructorName ||
           Kind == NameKind::CXXConversionFunctionName);
    return NamedType;
  }

  /// Get the name as a string for display.
  llvm::StringRef getAsString() const {
    if (isIdentifier())
      return Name;
    // For non-identifier names, return empty; callers should handle specially.
    return "";
  }

  bool isEmpty() const {
    return Kind == NameKind::Identifier && Name.empty();
  }

  bool operator==(const DeclarationName &RHS) const {
    if (Kind != RHS.Kind)
      return false;
    switch (Kind) {
    case NameKind::Identifier:
    case NameKind::CXXOperatorName:
    case NameKind::CXXLiteralOperatorName:
      return Name == RHS.Name;
    case NameKind::CXXConstructorName:
    case NameKind::CXXDestructorName:
    case NameKind::CXXConversionFunctionName:
      return NamedType == RHS.NamedType;
    case NameKind::CXXDeductionGuideName:
      return Name == RHS.Name;
    }
    return false;
  }

  bool operator!=(const DeclarationName &RHS) const { return !(*this == RHS); }
};

//===----------------------------------------------------------------------===//
// Declarator
//===----------------------------------------------------------------------===//

/// Declarator - Represents a fully parsed declarator.
///
/// Contains: DeclSpec (type + specifiers), a chain of DeclaratorChunks,
/// the declaration name, and the context where the declarator appears.
class Declarator {
  DeclSpec DS;
  llvm::SmallVector<DeclaratorChunk, 8> Chunks;
  DeclarationName Name;
  SourceLocation NameLoc;
  SourceRange Range;
  DeclaratorContext Context;

public:
  Declarator(const DeclSpec &DS, DeclaratorContext Ctx)
      : DS(DS), Context(Ctx) {}

  //===--------------------------------------------------------------------===//
  // Chunk management
  //===--------------------------------------------------------------------===//

  void addChunk(DeclaratorChunk C) { Chunks.push_back(std::move(C)); }

  unsigned getNumChunks() const { return Chunks.size(); }
  llvm::ArrayRef<DeclaratorChunk> chunks() const { return Chunks; }
  llvm::MutableArrayRef<DeclaratorChunk> chunks() { return Chunks; }

  //===--------------------------------------------------------------------===//
  // Name management
  //===--------------------------------------------------------------------===//

  void setName(DeclarationName N, SourceLocation Loc) {
    Name = N;
    NameLoc = Loc;
  }

  void setName(llvm::StringRef N, SourceLocation Loc) {
    Name = DeclarationName(N);
    NameLoc = Loc;
  }

  DeclarationName getName() const { return Name; }
  SourceLocation getNameLoc() const { return NameLoc; }

  /// Clear the name (for re-parsing in multi-declarator).
  void clearName() {
    Name = DeclarationName();
    NameLoc = SourceLocation();
  }

  //===--------------------------------------------------------------------===//
  // Range management
  //===--------------------------------------------------------------------===//

  void setRange(SourceRange R) { Range = R; }
  SourceRange getRange() const { return Range; }

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  const DeclSpec &getDeclSpec() const { return DS; }
  DeclSpec &getMutableDeclSpec() { return DS; }
  DeclaratorContext getContext() const { return Context; }

  //===--------------------------------------------------------------------===//
  // Query methods
  //===--------------------------------------------------------------------===//

  /// Returns true if this declarator has a function chunk (is a function).
  bool isFunctionDeclarator() const {
    for (const auto &C : Chunks)
      if (C.getKind() == DeclaratorChunk::Function)
        return true;
    return false;
  }

  /// Returns true if this declarator has any pointer or reference chunk.
  bool hasPointerOrReference() const {
    for (const auto &C : Chunks)
      if (C.getKind() == DeclaratorChunk::Pointer ||
          C.getKind() == DeclaratorChunk::Reference ||
          C.getKind() == DeclaratorChunk::MemberPointer)
        return true;
    return false;
  }

  /// Returns true if this has an array chunk.
  bool hasArray() const {
    for (const auto &C : Chunks)
      if (C.getKind() == DeclaratorChunk::Array)
        return true;
    return false;
  }

  bool hasName() const { return !Name.isEmpty(); }

  //===--------------------------------------------------------------------===//
  // Type building
  //===--------------------------------------------------------------------===//

  /// Build the final QualType from DeclSpec type + all chunks.
  /// Chunks are applied from outermost to innermost (reverse iteration).
  QualType buildType(ASTContext &Ctx) const;

  //===--------------------------------------------------------------------===//
  // Debug
  //===--------------------------------------------------------------------===//

  void dump(llvm::raw_ostream &OS, unsigned Indent = 0) const;
  void dump() const;
};

} // namespace blocktype
