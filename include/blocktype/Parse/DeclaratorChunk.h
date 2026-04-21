//===--- DeclaratorChunk.h - Declarator Chunks --------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the DeclaratorChunk class, which represents one level
// of a declarator (pointer, reference, array, function, or member pointer).
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceLocation.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace blocktype {

class Expr;
class ParmVarDecl;

/// DeclaratorChunk - One level of a declarator.
///
/// Chunks are stored in the Declarator from innermost to outermost.
/// For example, `int (*pf)(int)` has chunks: [Pointer, Function].
/// `int arr[10]` has chunks: [Array].
/// `int *ap[10]` has chunks: [Pointer, Array] (pointer inner, array outer).
/// `int (*arr)[10]` has chunks: [Pointer, Array] (pointer inner due to parens).
///
/// buildType() applies chunks in reverse order to build the final QualType.
class DeclaratorChunk {
public:
  enum ChunkKind {
    Pointer,       // * / *const / *volatile / *const volatile
    Reference,     // & / &&
    Array,         // [expr?] / [size]
    Function,      // (params) cvr-qualifiers ref-qualifier
    MemberPointer, // Class::*
  };

  /// FunctionInfo - Stores parsed function declarator information.
  struct FunctionInfo {
    llvm::SmallVector<ParmVarDecl *, 4> Params;
    bool IsVariadic = false;
    Qualifier MethodQuals = Qualifier::None;
    bool HasRefQualifier = false;
    bool IsRValueRef = false;
    Expr *NoexceptExpr = nullptr;
    bool HasNoexceptSpec = false;
    bool NoexceptValue = true;
    QualType TrailingReturnType;  // C++11 trailing return type (auto f() -> int)
    SourceLocation TrailingReturnLoc;  // Location of '->'
  };

private:
  ChunkKind Kind;

  // Pointer / MemberPointer qualifiers (const/volatile/restrict)
  Qualifier CVRQuals = Qualifier::None;

  // Source location of the chunk
  SourceLocation Loc;

  // Array info
  Expr *ArraySize = nullptr; // nullptr = incomplete array (int[])

  // Function info
  FunctionInfo Func;

  // MemberPointer class qualifier
  llvm::StringRef ClassQualifier;

  // Reference: is rvalue reference?
  bool IsRValueRef = false;

public:
  ChunkKind getKind() const { return Kind; }
  SourceLocation getLoc() const { return Loc; }
  Qualifier getCVRQualifiers() const { return CVRQuals; }

  //===--------------------------------------------------------------------===//
  // Factory methods
  //===--------------------------------------------------------------------===//

  static DeclaratorChunk getPointer(Qualifier CVR, SourceLocation Loc) {
    DeclaratorChunk C;
    C.Kind = Pointer;
    C.CVRQuals = CVR;
    C.Loc = Loc;
    return C;
  }

  static DeclaratorChunk getReference(bool IsRValue, SourceLocation Loc) {
    DeclaratorChunk C;
    C.Kind = Reference;
    C.IsRValueRef = IsRValue;
    C.Loc = Loc;
    return C;
  }

  static DeclaratorChunk getArray(Expr *Size, SourceLocation Loc) {
    DeclaratorChunk C;
    C.Kind = Array;
    C.ArraySize = Size;
    C.Loc = Loc;
    return C;
  }

  static DeclaratorChunk getFunction(FunctionInfo FI, SourceLocation Loc) {
    DeclaratorChunk C;
    C.Kind = Function;
    C.Func = std::move(FI);
    C.Loc = Loc;
    return C;
  }

  static DeclaratorChunk getMemberPointer(llvm::StringRef Class,
                                          Qualifier CVR,
                                          SourceLocation Loc) {
    DeclaratorChunk C;
    C.Kind = MemberPointer;
    C.ClassQualifier = Class;
    C.CVRQuals = CVR;
    C.Loc = Loc;
    return C;
  }

  //===--------------------------------------------------------------------===//
  // Accessors
  //===--------------------------------------------------------------------===//

  /// Array accessors
  Expr *getArraySize() const {
    assert(Kind == Array);
    return ArraySize;
  }

  /// Function accessors
  const FunctionInfo &getFunctionInfo() const {
    assert(Kind == Function);
    return Func;
  }
  FunctionInfo &getFunctionInfo() {
    assert(Kind == Function);
    return Func;
  }

  /// Reference accessors
  bool isRValueReference() const {
    assert(Kind == Reference);
    return IsRValueRef;
  }

  /// MemberPointer accessors
  llvm::StringRef getClassQualifier() const {
    assert(Kind == MemberPointer);
    return ClassQualifier;
  }
};

} // namespace blocktype
