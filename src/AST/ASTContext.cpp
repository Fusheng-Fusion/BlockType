//===--- ASTContext.cpp - AST Context Implementation ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Type.h"

namespace blocktype {

//===----------------------------------------------------------------------===//
// Type Creation
//===----------------------------------------------------------------------===//

BuiltinType *ASTContext::getBuiltinType(BuiltinKind Kind) {
  unsigned Index = static_cast<unsigned>(Kind);
  
  // Check cache
  if (Index < static_cast<unsigned>(BuiltinKind::NumBuiltinTypes) && BuiltinTypes[Index]) {
    return BuiltinTypes[Index];
  }
  
  // Create new builtin type
  void *Mem = Allocator.Allocate(sizeof(BuiltinType), alignof(BuiltinType));
  BuiltinType *BT = new (Mem) BuiltinType(Kind);
  
  // Cache it
  if (Index < static_cast<unsigned>(BuiltinKind::NumBuiltinTypes)) {
    BuiltinTypes[Index] = BT;
  }
  
  return BT;
}

PointerType *ASTContext::getPointerType(const Type *Pointee) {
  // Create new pointer type (no deduplication for simplicity)
  void *Mem = Allocator.Allocate(sizeof(PointerType), alignof(PointerType));
  PointerType *PT = new (Mem) PointerType(Pointee);
  return PT;
}

LValueReferenceType *ASTContext::getLValueReferenceType(const Type *Referenced) {
  // Create new reference type (no deduplication for simplicity)
  void *Mem = Allocator.Allocate(sizeof(LValueReferenceType), alignof(LValueReferenceType));
  LValueReferenceType *RT = new (Mem) LValueReferenceType(Referenced);
  return RT;
}

RValueReferenceType *ASTContext::getRValueReferenceType(const Type *Referenced) {
  // Create new reference type (no deduplication for simplicity)
  void *Mem = Allocator.Allocate(sizeof(RValueReferenceType), alignof(RValueReferenceType));
  RValueReferenceType *RT = new (Mem) RValueReferenceType(Referenced);
  return RT;
}

ArrayType *ASTContext::getArrayType(const Type *Element, Expr *Size) {
  // Create new array type (no deduplication for simplicity)
  void *Mem = Allocator.Allocate(sizeof(ArrayType), alignof(ArrayType));
  ArrayType *AT = new (Mem) ArrayType(Element, Size);
  return AT;
}

ASTContext::~ASTContext() {
  destroyAll();
}

void ASTContext::clear() {
  destroyAll();
  Nodes.clear();
  Allocator.Reset();
}

void ASTContext::destroyAll() {
  // Destroy in reverse order of creation
  for (auto It = Nodes.rbegin(); It != Nodes.rend(); ++It) {
    ASTNode *Node = *It;
    Node->~ASTNode();
  }
}

void ASTContext::dumpMemoryUsage(raw_ostream &OS) const {
  OS << "AST Memory Usage:\n";
  OS << "  Nodes allocated: " << Nodes.size() << "\n";
  OS << "  Total memory: " << getMemoryUsage() << " bytes\n";
  OS << "  Allocator overhead: " 
     << (getMemoryUsage() - Nodes.size() * sizeof(ASTNode)) << " bytes\n";
}

} // namespace blocktype
