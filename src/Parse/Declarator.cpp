//===--- Declarator.cpp - Declarator Implementation ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements Declarator::buildType() and dump().
//
//===----------------------------------------------------------------------===//

#include "blocktype/Parse/Declarator.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

QualType Declarator::buildType(ASTContext &Ctx) const {
  QualType Result = DS.Type;
  if (Result.isNull())
    return QualType();

  // Apply chunks from first to last (type-application order).
  //
  // Chunk ordering convention:
  //   `int *ap[10]`  →  chunks: [Pointer, Array]
  //     Forward: int → int* → (int*)[]  = array of pointers ✓
  //
  //   `int (*arr)[10]`  →  chunks: [Array, Pointer]
  //     (Array inserted before Pointer due to paren-changed binding)
  //     Forward: int → int[10] → int(*)[10]  = pointer to array ✓
  //
  //   `int (*pf)(int)`  →  chunks: [Function, Pointer]
  //     Forward: int → int(int) → int(*)(int)  = pointer to function ✓
  for (const auto &C : Chunks) {
    switch (C.getKind()) {
    case DeclaratorChunk::Pointer: {
      PointerType *PT = Ctx.getPointerType(Result.getTypePtr());
      Result = QualType(PT, C.getCVRQualifiers());
      break;
    }
    case DeclaratorChunk::Reference: {
      if (C.isRValueReference()) {
        auto *RT = Ctx.getRValueReferenceType(Result.getTypePtr());
        Result = QualType(RT, Qualifier::None);
      } else {
        auto *RT = Ctx.getLValueReferenceType(Result.getTypePtr());
        Result = QualType(RT, Qualifier::None);
      }
      break;
    }
    case DeclaratorChunk::Array: {
      ArrayType *AT = Ctx.getArrayType(Result.getTypePtr(), C.getArraySize());
      Result = QualType(AT, Qualifier::None);
      break;
    }
    case DeclaratorChunk::Function: {
      const auto &FI = C.getFunctionInfo();
      llvm::SmallVector<const Type *, 8> ParamTypes;
      for (auto *P : FI.Params)
        ParamTypes.push_back(P->getType().getTypePtr());
      auto *FT = Ctx.getFunctionType(Result.getTypePtr(), ParamTypes,
                                      FI.IsVariadic);
      Result = QualType(FT, FI.MethodQuals);
      break;
    }
    case DeclaratorChunk::MemberPointer: {
      auto *ClassType = Ctx.getUnresolvedType(C.getClassQualifier());
      auto *MPT = Ctx.getMemberPointerType(ClassType, Result.getTypePtr());
      Result = QualType(MPT, C.getCVRQualifiers());
      break;
    }
    }
  }

  return Result;
}

void Declarator::dump(llvm::raw_ostream &OS, unsigned Indent) const {
  OS.indent(Indent) << "Declarator";
  if (hasName())
    OS << " '" << Name.getAsString() << "'";
  OS << "\n";

  OS.indent(Indent + 2) << "DeclSpec: ";
  if (DS.hasTypeSpecifier()) {
    OS << "type='";
    DS.Type.dump(OS);
    OS << "'";
  }
  if (DS.SC != StorageClass::None) {
    switch (DS.SC) {
    case StorageClass::Static:     OS << " static"; break;
    case StorageClass::Extern:     OS << " extern"; break;
    case StorageClass::Mutable:    OS << " mutable"; break;
    case StorageClass::ThreadLocal: OS << " thread_local"; break;
    case StorageClass::Register:   OS << " register"; break;
    default: break;
    }
  }
  if (DS.IsInline)    OS << " inline";
  if (DS.IsVirtual)   OS << " virtual";
  if (DS.IsExplicit)  OS << " explicit";
  if (DS.IsFriend)    OS << " friend";
  if (DS.IsConstexpr) OS << " constexpr";
  if (DS.IsConsteval) OS << " consteval";
  if (DS.IsConstinit) OS << " constinit";
  if (DS.IsTypedef)   OS << " typedef";
  OS << "\n";

  for (const auto &C : Chunks) {
    OS.indent(Indent + 2);
    switch (C.getKind()) {
    case DeclaratorChunk::Pointer: {
      OS << "Chunk: Pointer";
      auto Q = C.getCVRQualifiers();
      if (hasQualifier(Q, Qualifier::Const))    OS << " const";
      if (hasQualifier(Q, Qualifier::Volatile)) OS << " volatile";
      if (hasQualifier(Q, Qualifier::Restrict)) OS << " restrict";
      OS << "\n";
      break;
    }
    case DeclaratorChunk::Reference:
      OS << "Chunk: " << (C.isRValueReference() ? "RValueRef" : "LValueRef")
         << "\n";
      break;
    case DeclaratorChunk::Array:
      OS << "Chunk: Array\n";
      break;
    case DeclaratorChunk::Function:
      OS << "Chunk: Function, " << C.getFunctionInfo().Params.size()
         << " params\n";
      break;
    case DeclaratorChunk::MemberPointer:
      OS << "Chunk: MemberPointer, class=" << C.getClassQualifier() << "\n";
      break;
    }
  }
}

void Declarator::dump() const { dump(llvm::errs()); }

} // namespace blocktype
