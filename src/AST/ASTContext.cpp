//===--- ASTContext.cpp - AST Context Implementation ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Type.h"
#include "blocktype/AST/Decl.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/StringRef.h"

namespace blocktype {

using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

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
  // For backward compatibility, create a constant array type
  // This method is kept for compatibility with existing code
  if (Size) {
    // Try to evaluate the size expression
    // For now, create an incomplete array type
    return getIncompleteArrayType(Element);
  }
  return getIncompleteArrayType(Element);
}

ConstantArrayType *ASTContext::getConstantArrayType(const Type *Element,
                                                     Expr *SizeExpr,
                                                     llvm::APInt Size) {
  void *Mem = Allocator.Allocate(sizeof(ConstantArrayType),
                                   alignof(ConstantArrayType));
  auto *AT = new (Mem) ConstantArrayType(Element, SizeExpr, Size);
  return AT;
}

IncompleteArrayType *ASTContext::getIncompleteArrayType(const Type *Element) {
  void *Mem = Allocator.Allocate(sizeof(IncompleteArrayType),
                                   alignof(IncompleteArrayType));
  auto *AT = new (Mem) IncompleteArrayType(Element);
  return AT;
}

VariableArrayType *ASTContext::getVariableArrayType(const Type *Element,
                                                     Expr *SizeExpr) {
  void *Mem = Allocator.Allocate(sizeof(VariableArrayType),
                                   alignof(VariableArrayType));
  auto *AT = new (Mem) VariableArrayType(Element, SizeExpr);
  return AT;
}

TemplateTypeParmType *ASTContext::getTemplateTypeParmType(TemplateTypeParmDecl *Decl,
                                                           unsigned Index,
                                                           unsigned Depth,
                                                           bool IsPack) {
  void *Mem = Allocator.Allocate(sizeof(TemplateTypeParmType),
                                   alignof(TemplateTypeParmType));
  auto *TPT = new (Mem) TemplateTypeParmType(Decl, Index, Depth, IsPack);
  return TPT;
}

DependentType *ASTContext::getDependentType(const Type *BaseType,
                                             llvm::StringRef Name) {
  void *Mem = Allocator.Allocate(sizeof(DependentType), alignof(DependentType));
  auto *DT = new (Mem) DependentType(BaseType, saveString(Name));
  return DT;
}

UnresolvedType *ASTContext::getUnresolvedType(llvm::StringRef Name) {
  // Create new unresolved type
  void *Mem = Allocator.Allocate(sizeof(UnresolvedType), alignof(UnresolvedType));
  auto *Unresolved = new (Mem) UnresolvedType(Name);
  return Unresolved;
}

TemplateSpecializationType *ASTContext::getTemplateSpecializationType(llvm::StringRef Name) {
  // Create new template specialization type
  void *Mem = Allocator.Allocate(sizeof(TemplateSpecializationType), alignof(TemplateSpecializationType));
  auto *Template = new (Mem) TemplateSpecializationType(Name);
  return Template;
}

ElaboratedType *ASTContext::getElaboratedType(const Type *NamedType, llvm::StringRef Qualifier) {
  // Create new elaborated type
  void *Mem = Allocator.Allocate(sizeof(ElaboratedType), alignof(ElaboratedType));
  auto *Elaborated = new (Mem) ElaboratedType(NamedType, Qualifier);
  return Elaborated;
}

DecltypeType *ASTContext::getDecltypeType(Expr *E, QualType Underlying) {
  // Create new decltype type
  void *Mem = Allocator.Allocate(sizeof(DecltypeType), alignof(DecltypeType));
  auto *Decltype = new (Mem) DecltypeType(E, Underlying);
  return Decltype;
}

MemberPointerType *ASTContext::getMemberPointerType(const Type *ClassType, const Type *PointeeType) {
  // Create new member pointer type
  void *Mem = Allocator.Allocate(sizeof(MemberPointerType), alignof(MemberPointerType));
  auto *MemberPtr = new (Mem) MemberPointerType(ClassType, PointeeType);
  return MemberPtr;
}

FunctionType *ASTContext::getFunctionType(const Type *ReturnType,
                                          llvm::ArrayRef<const Type *> ParamTypes,
                                          bool IsVariadic) {
  // Create new function type
  void *Mem = Allocator.Allocate(sizeof(FunctionType), alignof(FunctionType));
  auto *Func = new (Mem) FunctionType(ReturnType, ParamTypes, IsVariadic);
  return Func;
}

QualType ASTContext::getTypeDeclType(const TypeDecl *D) {
  // Check the specific type of TypeDecl
  if (auto *RD = dyn_cast<RecordDecl>(D)) {
    // Create RecordType
    void *Mem = Allocator.Allocate(sizeof(RecordType), alignof(RecordType));
    auto *RT = new (Mem) RecordType(const_cast<RecordDecl*>(RD));
    return QualType(RT, Qualifier::None);
  }
  
  if (auto *ED = dyn_cast<EnumDecl>(D)) {
    // Create EnumType
    void *Mem = Allocator.Allocate(sizeof(EnumType), alignof(EnumType));
    auto *ET = new (Mem) EnumType(const_cast<EnumDecl*>(ED));
    return QualType(ET, Qualifier::None);
  }
  
  if (auto *TND = dyn_cast<TypedefNameDecl>(D)) {
    // Create TypedefType
    void *Mem = Allocator.Allocate(sizeof(TypedefType), alignof(TypedefType));
    auto *TT = new (Mem) TypedefType(const_cast<TypedefNameDecl*>(TND));
    return QualType(TT, Qualifier::None);
  }

  // For other TypeDecls (e.g., TemplateTypeParmDecl), create an unresolved type
  return QualType(getUnresolvedType(D->getName()), Qualifier::None);
}

QualType ASTContext::getAutoType() {
  // Create AutoType
  void *Mem = Allocator.Allocate(sizeof(AutoType), alignof(AutoType));
  auto *AT = new (Mem) AutoType();
  return QualType(AT, Qualifier::None);
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
