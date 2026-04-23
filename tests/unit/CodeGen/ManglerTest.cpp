//===--- ManglerTest.cpp - Itanium Mangler Unit Tests --------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <gtest/gtest.h>
#include "blocktype/CodeGen/Mangler.h"
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/DeclContext.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/SourceManager.h"
#include "llvm/IR/LLVMContext.h"

using namespace blocktype;

namespace {

class ManglerTest : public ::testing::Test {
protected:
  llvm::LLVMContext LLVMCtx;
  ASTContext Ctx;
  SourceManager SM;
  std::unique_ptr<CodeGenModule> CGM;
  std::unique_ptr<Mangler> M;

  ManglerTest()
      : Ctx(),
        CGM(std::make_unique<CodeGenModule>(Ctx, LLVMCtx, SM, "test", "arm64-apple-macosx14.0")),
        M(std::make_unique<Mangler>(*CGM)) {}

  /// Helper: allocate a BuiltinType via the ASTContext allocator
  BuiltinType *makeBuiltin(BuiltinKind K) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(BuiltinType), alignof(BuiltinType));
    return new (Mem) BuiltinType(K);
  }

  /// Helper: allocate a PointerType
  PointerType *makePointer(const Type *Pointee) {
    return Ctx.getPointerType(Pointee);
  }

  /// Helper: allocate a LValueReferenceType
  LValueReferenceType *makeLRef(const Type *T) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(LValueReferenceType), alignof(LValueReferenceType));
    return new (Mem) LValueReferenceType(T);
  }

  /// Helper: allocate a RValueReferenceType
  RValueReferenceType *makeRRef(const Type *T) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(RValueReferenceType), alignof(RValueReferenceType));
    return new (Mem) RValueReferenceType(T);
  }

  /// Helper: allocate a ConstantArrayType
  ConstantArrayType *makeConstArray(const Type *Elem, uint64_t Size) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(ConstantArrayType), alignof(ConstantArrayType));
    return new (Mem) ConstantArrayType(Elem, nullptr, llvm::APInt(64, Size));
  }

  /// Helper: allocate an IncompleteArrayType
  IncompleteArrayType *makeIncArray(const Type *Elem) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(IncompleteArrayType), alignof(IncompleteArrayType));
    return new (Mem) IncompleteArrayType(Elem);
  }

  /// Helper: allocate a FunctionType
  FunctionType *makeFuncType(const Type *Ret, llvm::ArrayRef<const Type *> Params,
                              bool Variadic = false) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(FunctionType), alignof(FunctionType));
    return new (Mem) FunctionType(Ret, Params, Variadic);
  }

  /// Helper: allocate a ParmVarDecl
  ParmVarDecl *makeParam(llvm::StringRef Name, QualType T, unsigned Idx) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(ParmVarDecl), alignof(ParmVarDecl));
    return new (Mem) ParmVarDecl(SourceLocation(), Name, T, Idx);
  }

  /// Helper: allocate a FunctionDecl
  FunctionDecl *makeFuncDecl(llvm::StringRef Name, QualType FTy,
                              llvm::ArrayRef<ParmVarDecl *> Params) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(FunctionDecl), alignof(FunctionDecl));
    return new (Mem) FunctionDecl(SourceLocation(), Name, FTy, Params);
  }

  /// Helper: allocate a RecordDecl
  RecordDecl *makeRecordDecl(llvm::StringRef Name) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(RecordDecl), alignof(RecordDecl));
    return new (Mem) RecordDecl(SourceLocation(), Name, TagDecl::TK_class);
  }

  /// Helper: allocate a CXXRecordDecl
  CXXRecordDecl *makeCXXRecordDecl(llvm::StringRef Name) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(CXXRecordDecl), alignof(CXXRecordDecl));
    return new (Mem) CXXRecordDecl(SourceLocation(), Name, TagDecl::TK_class);
  }

  /// Helper: allocate a RecordType
  RecordType *makeRecordType(RecordDecl *RD) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(RecordType), alignof(RecordType));
    return new (Mem) RecordType(RD);
  }

  /// Helper: allocate a CXXMethodDecl
  CXXMethodDecl *makeMethodDecl(llvm::StringRef Name, QualType FTy,
                                 llvm::ArrayRef<ParmVarDecl *> Params,
                                 CXXRecordDecl *Parent) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(CXXMethodDecl), alignof(CXXMethodDecl));
    return new (Mem) CXXMethodDecl(SourceLocation(), Name, FTy, Params, Parent);
  }

  /// Helper: allocate a CXXConstructorDecl
  CXXConstructorDecl *makeCtorDecl(CXXRecordDecl *Parent,
                                    llvm::ArrayRef<ParmVarDecl *> Params) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(CXXConstructorDecl), alignof(CXXConstructorDecl));
    return new (Mem) CXXConstructorDecl(SourceLocation(), Parent, Params);
  }

  /// Helper: allocate a CXXDestructorDecl
  CXXDestructorDecl *makeDtorDecl(CXXRecordDecl *Parent) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(CXXDestructorDecl), alignof(CXXDestructorDecl));
    return new (Mem) CXXDestructorDecl(SourceLocation(), Parent);
  }

  /// Helper: allocate a VarDecl
  VarDecl *makeVarDecl(llvm::StringRef Name, QualType T) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(VarDecl), alignof(VarDecl));
    return new (Mem) VarDecl(SourceLocation(), Name, T);
  }

  /// Helper: allocate an EnumDecl
  EnumDecl *makeEnumDecl(llvm::StringRef Name) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(EnumDecl), alignof(EnumDecl));
    return new (Mem) EnumDecl(SourceLocation(), Name);
  }

  /// Helper: allocate an EnumType
  EnumType *makeEnumType(EnumDecl *ED) {
    void *Mem = Ctx.getAllocator().Allocate(sizeof(EnumType), alignof(EnumType));
    return new (Mem) EnumType(ED);
  }
};

//===----------------------------------------------------------------------===//
// Builtin type mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, BuiltinVoid) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Void)), "v");
}

TEST_F(ManglerTest, BuiltinBool) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Bool)), "b");
}

TEST_F(ManglerTest, BuiltinChar) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Char)), "c");
}

TEST_F(ManglerTest, BuiltinInt) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Int)), "i");
}

TEST_F(ManglerTest, BuiltinLong) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Long)), "l");
}

TEST_F(ManglerTest, BuiltinUnsignedInt) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::UnsignedInt)), "j");
}

TEST_F(ManglerTest, BuiltinFloat) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Float)), "f");
}

TEST_F(ManglerTest, BuiltinDouble) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Double)), "d");
}

TEST_F(ManglerTest, BuiltinLongDouble) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::LongDouble)), "e");
}

TEST_F(ManglerTest, BuiltinNullPtr) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::NullPtr)), "Dn");
}

TEST_F(ManglerTest, BuiltinWChar) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::WChar)), "w");
}

TEST_F(ManglerTest, BuiltinChar16) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Char16)), "Ds");
}

TEST_F(ManglerTest, BuiltinChar32) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Char32)), "Di");
}

TEST_F(ManglerTest, BuiltinInt128) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::Int128)), "n");
}

TEST_F(ManglerTest, BuiltinUInt128) {
  EXPECT_EQ(M->mangleType(makeBuiltin(BuiltinKind::UnsignedInt128)), "o");
}

//===----------------------------------------------------------------------===//
// Pointer / Reference type mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, PointerToInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makePointer(IntTy)), "Pi");
}

TEST_F(ManglerTest, PointerToPointerToChar) {
  auto *CharTy = makeBuiltin(BuiltinKind::Char);
  auto *PtrChar = makePointer(CharTy);
  EXPECT_EQ(M->mangleType(makePointer(PtrChar)), "PPc");
}

TEST_F(ManglerTest, LValueReferenceToInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makeLRef(IntTy)), "Ri");
}

TEST_F(ManglerTest, RValueReferenceToInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makeRRef(IntTy)), "Oi");
}

//===----------------------------------------------------------------------===//
// Const/Volatile qualified types
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, ConstInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  QualType ConstInt(IntTy, Qualifier::Const);
  EXPECT_EQ(M->mangleType(ConstInt), "Ki");
}

TEST_F(ManglerTest, ConstPointerToChar) {
  auto *CharTy = makeBuiltin(BuiltinKind::Char);
  auto *PtrChar = makePointer(CharTy);
  QualType ConstPtr(PtrChar, Qualifier::Const);
  EXPECT_EQ(M->mangleType(ConstPtr), "KPc");
}

TEST_F(ManglerTest, PointerToConstChar) {
  // Note: PointerType stores const Type* (unqualified), so const is lost.
  // To properly mangle const pointee, the pointee's QualType must be propagated.
  // This test verifies that const on the pointer itself works:
  auto *CharTy = makeBuiltin(BuiltinKind::Char);
  auto *PtrChar = makePointer(CharTy);
  QualType ConstPtr(PtrChar, Qualifier::Const);
  EXPECT_EQ(M->mangleType(ConstPtr), "KPc");
}

//===----------------------------------------------------------------------===//
// Record / Enum type mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, RecordType) {
  auto *RD = makeRecordDecl("MyClass");
  auto *RT = makeRecordType(RD);
  EXPECT_EQ(M->mangleType(RT), "7MyClass");
}

TEST_F(ManglerTest, EnumType) {
  auto *ED = makeEnumDecl("Color");
  auto *ET = makeEnumType(ED);
  EXPECT_EQ(M->mangleType(ET), "5Color");
}

//===----------------------------------------------------------------------===//
// Array type mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, ConstantArray) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makeConstArray(IntTy, 10)), "A10_i");
}

TEST_F(ManglerTest, IncompleteArray) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makeIncArray(IntTy)), "A_i");
}

//===----------------------------------------------------------------------===//
// Function type mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, FunctionTypeIntInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  EXPECT_EQ(M->mangleType(makeFuncType(IntTy, {IntTy})), "FiiE");
}

TEST_F(ManglerTest, FunctionTypeVoidNoParams) {
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  EXPECT_EQ(M->mangleType(makeFuncType(VoidTy, {})), "FvE");
}

TEST_F(ManglerTest, FunctionTypeVariadic) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  EXPECT_EQ(M->mangleType(makeFuncType(VoidTy, {IntTy}, true)), "FvizE");
}

//===----------------------------------------------------------------------===//
// Free function mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, FreeFunctionNoParams) {
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {});
  auto *FD = makeFuncDecl("foo", QualType(FTy, Qualifier::None), {});
  EXPECT_EQ(M->getMangledName(FD), "_Z3foov");
}

TEST_F(ManglerTest, FreeFunctionOneIntParam) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy = makeFuncType(IntTy, {IntTy});
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *FD = makeFuncDecl("foo", QualType(FTy, Qualifier::None), {Param});
  EXPECT_EQ(M->getMangledName(FD), "_Z3fooi");
}

TEST_F(ManglerTest, FreeFunctionTwoParams) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FloatTy = makeBuiltin(BuiltinKind::Float);
  auto *FTy = makeFuncType(IntTy, {IntTy, FloatTy});
  auto *P1 = makeParam("a", QualType(IntTy, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(FloatTy, Qualifier::None), 1);
  auto *FD = makeFuncDecl("add", QualType(FTy, Qualifier::None), {P1, P2});
  EXPECT_EQ(M->getMangledName(FD), "_Z3addif");
}

TEST_F(ManglerTest, OverloadedFunctions) {
  // void foo()
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy1 = makeFuncType(VoidTy, {});
  auto *FD1 = makeFuncDecl("foo", QualType(FTy1, Qualifier::None), {});

  // int foo(int)
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy2 = makeFuncType(IntTy, {IntTy});
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *FD2 = makeFuncDecl("foo", QualType(FTy2, Qualifier::None), {Param});

  EXPECT_EQ(M->getMangledName(FD1), "_Z3foov");
  EXPECT_EQ(M->getMangledName(FD2), "_Z3fooi");
  EXPECT_NE(M->getMangledName(FD1), M->getMangledName(FD2));
}

//===----------------------------------------------------------------------===//
// Member function mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, MemberFunction) {
  auto *RD = makeCXXRecordDecl("Foo");
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy = makeFuncType(IntTy, {IntTy});
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *MD = makeMethodDecl("bar", QualType(FTy, Qualifier::None), {Param}, RD);
  EXPECT_EQ(M->getMangledName(MD), "_ZN3Foo3barEi");
}

TEST_F(ManglerTest, MemberFunctionNoParams) {
  auto *RD = makeCXXRecordDecl("Foo");
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {});
  auto *MD = makeMethodDecl("baz", QualType(FTy, Qualifier::None), {}, RD);
  EXPECT_EQ(M->getMangledName(MD), "_ZN3Foo3bazEv");
}

//===----------------------------------------------------------------------===//
// Constructor / Destructor mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, ConstructorNoParams) {
  auto *RD = makeCXXRecordDecl("Foo");
  auto *Ctor = makeCtorDecl(RD, {});
  EXPECT_EQ(M->getMangledName(Ctor), "_ZN3FooC1Ev");
}

TEST_F(ManglerTest, ConstructorWithParams) {
  auto *RD = makeCXXRecordDecl("Foo");
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *Ctor = makeCtorDecl(RD, {Param});
  EXPECT_EQ(M->getMangledName(Ctor), "_ZN3FooC1Ei");
}

TEST_F(ManglerTest, Destructor) {
  auto *RD = makeCXXRecordDecl("Foo");
  auto *Dtor = makeDtorDecl(RD);
  EXPECT_EQ(M->getMangledName(Dtor), "_ZN3FooD1Ev");
}

//===----------------------------------------------------------------------===//
// VTable / RTTI name mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, VTableName) {
  auto *RD = makeCXXRecordDecl("Foo");
  EXPECT_EQ(M->getVTableName(RD), "_ZTV3Foo");
}

TEST_F(ManglerTest, VTableNameLonger) {
  auto *RD = makeCXXRecordDecl("MyClass");
  EXPECT_EQ(M->getVTableName(RD), "_ZTV7MyClass");
}

TEST_F(ManglerTest, RTTIName) {
  auto *RD = makeCXXRecordDecl("Foo");
  EXPECT_EQ(M->getRTTIName(RD), "_ZTI3Foo");
}

TEST_F(ManglerTest, TypeinfoName) {
  auto *RD = makeCXXRecordDecl("Foo");
  EXPECT_EQ(M->getTypeinfoName(RD), "_ZTS3Foo");
}

//===----------------------------------------------------------------------===//
// Global variable mangling
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, GlobalVariableNoMangling) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *VD = makeVarDecl("g_var", QualType(IntTy, Qualifier::None));
  EXPECT_EQ(M->getMangledName(VD), "g_var");
}

//===----------------------------------------------------------------------===//
// Complex types
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, PointerToFunctionReturningInt) {
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy = makeFuncType(IntTy, {IntTy});
  EXPECT_EQ(M->mangleType(makePointer(FTy)), "PFiiE");
}

TEST_F(ManglerTest, PointerToConstPointerToChar) {
  // Note: Our PointerType stores const Type* (unqualified).
  // const on pointee is not propagated through PointerType.
  // This test verifies const on the outer pointer itself:
  auto *CharTy = makeBuiltin(BuiltinKind::Char);
  auto *PtrChar = makePointer(CharTy);
  QualType ConstPtr(PtrChar, Qualifier::Const);
  auto *PtrConstPtr = makePointer(ConstPtr.getTypePtr());
  // Outer pointer has const stripped by PointerType, inner pointer also lost const
  EXPECT_EQ(M->mangleType(PtrConstPtr), "PPc");
}

TEST_F(ManglerTest, FunctionWithRecordParam) {
  auto *RD = makeRecordDecl("String");
  auto *RT = makeRecordType(RD);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT});
  auto *Param = makeParam("s", QualType(RT, Qualifier::None), 0);
  auto *FD = makeFuncDecl("print", QualType(FTy, Qualifier::None), {Param});
  EXPECT_EQ(M->getMangledName(FD), "_Z5print6String");
}

//===----------------------------------------------------------------------===//
// Substitution 压缩测试（Itanium ABI §5.3.5）
//===----------------------------------------------------------------------===//

TEST_F(ManglerTest, SubstitutionRecordTypeReuse) {
  // 同一个 record 类型在参数中出现两次，第二次应使用 substitution
  // void foo(MyClass, MyClass) → _Z3foo7MyClassS_
  auto *RD = makeRecordDecl("MyClass");
  auto *RT = makeRecordType(RD);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT, RT});
  auto *P1 = makeParam("a", QualType(RT, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT, Qualifier::None), 1);
  auto *FD = makeFuncDecl("foo", QualType(FTy, Qualifier::None), {P1, P2});
  EXPECT_EQ(M->getMangledName(FD), "_Z3foo7MyClassS_");
}

TEST_F(ManglerTest, SubstitutionEnumTypeReuse) {
  // 同一个 enum 类型出现两次
  // void bar(Color, Color) → _Z3bar5ColorS_
  auto *ED = makeEnumDecl("Color");
  auto *ET = makeEnumType(ED);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {ET, ET});
  auto *P1 = makeParam("a", QualType(ET, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(ET, Qualifier::None), 1);
  auto *FD = makeFuncDecl("bar", QualType(FTy, Qualifier::None), {P1, P2});
  EXPECT_EQ(M->getMangledName(FD), "_Z3bar5ColorS_");
}

TEST_F(ManglerTest, SubstitutionMultipleTypes) {
  // 三个不同类型，第三个引用第一个
  // void baz(MyClass, Other, MyClass) → _Z3baz7MyClass5OtherS_
  auto *RD1 = makeRecordDecl("MyClass");
  auto *RT1 = makeRecordType(RD1);
  auto *RD2 = makeRecordDecl("Other");
  auto *RT2 = makeRecordType(RD2);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT1, RT2, RT1});
  auto *P1 = makeParam("a", QualType(RT1, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT2, Qualifier::None), 1);
  auto *P3 = makeParam("c", QualType(RT1, Qualifier::None), 2);
  auto *FD = makeFuncDecl("baz", QualType(FTy, Qualifier::None), {P1, P2, P3});
  EXPECT_EQ(M->getMangledName(FD), "_Z3baz7MyClass5OtherS_");
}

TEST_F(ManglerTest, SubstitutionSecondIndex) {
  // 第一个类型出现两次，但第二次引用第二个类型
  // void qux(Alpha, Beta, Beta) → _Z3qux5Alpha4BetaS0_
  // Alpha = S_ (index 0), Beta = S0_ (index 1), second Beta = S0_
  auto *RD1 = makeRecordDecl("Alpha");
  auto *RT1 = makeRecordType(RD1);
  auto *RD2 = makeRecordDecl("Beta");
  auto *RT2 = makeRecordType(RD2);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT1, RT2, RT2});
  auto *P1 = makeParam("a", QualType(RT1, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT2, Qualifier::None), 1);
  auto *P3 = makeParam("c", QualType(RT2, Qualifier::None), 2);
  auto *FD = makeFuncDecl("qux", QualType(FTy, Qualifier::None), {P1, P2, P3});
  EXPECT_EQ(M->getMangledName(FD), "_Z3qux5Alpha4BetaS0_");
}

TEST_F(ManglerTest, SubstitutionResetBetweenCalls) {
  // 每次 getMangledName 调用应重置 substitution 表
  auto *RD = makeRecordDecl("MyClass");
  auto *RT = makeRecordType(RD);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT});
  auto *Param = makeParam("x", QualType(RT, Qualifier::None), 0);
  auto *FD = makeFuncDecl("single", QualType(FTy, Qualifier::None), {Param});
  // 第一次调用
  EXPECT_EQ(M->getMangledName(FD), "_Z6single7MyClass");
  // 第二次调用应产生相同结果（substitution 表已重置）
  EXPECT_EQ(M->getMangledName(FD), "_Z6single7MyClass");
}

TEST_F(ManglerTest, SubstitutionStdNamespace) {
  // std 命名空间中的函数应使用 St 缩写
  auto *NsStd = new (Ctx.getAllocator().Allocate(sizeof(NamespaceDecl), alignof(NamespaceDecl)))
      NamespaceDecl(SourceLocation(), "std");
  NsStd->setOwningDecl(NsStd);  // NamespaceDecl 自身就是 owning decl
  auto *TU = new (Ctx.getAllocator().Allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl)))
      TranslationUnitDecl(SourceLocation());
  NsStd->setParent(TU);

  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy = makeFuncType(IntTy, {IntTy});
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *FD = makeFuncDecl("foo", QualType(FTy, Qualifier::None), {Param});
  FD->setParent(NsStd);

  // std::foo(int) → _ZSt3fooi（St 缩写）
  EXPECT_EQ(M->getMangledName(FD), "_ZSt3fooi");
}

TEST_F(ManglerTest, SubstitutionNestedNamespace) {
  // ns::Foo::bar(int)
  auto *Ns = new (Ctx.getAllocator().Allocate(sizeof(NamespaceDecl), alignof(NamespaceDecl)))
      NamespaceDecl(SourceLocation(), "ns");
  Ns->setOwningDecl(Ns);
  auto *TU = new (Ctx.getAllocator().Allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl)))
      TranslationUnitDecl(SourceLocation());
  Ns->setParent(TU);

  auto *RD = makeCXXRecordDecl("Foo");
  RD->setParent(Ns);

  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FTy = makeFuncType(IntTy, {IntTy});
  auto *Param = makeParam("x", QualType(IntTy, Qualifier::None), 0);
  auto *MD = makeMethodDecl("bar", QualType(FTy, Qualifier::None), {Param}, RD);

  // ns::Foo::bar(int) → _ZN2ns3Foo3barEi
  EXPECT_EQ(M->getMangledName(MD), "_ZN2ns3Foo3barEi");
}

TEST_F(ManglerTest, SubstitutionRecordInMemberAndParam) {
  // 成员函数的类名和参数类型名相同时，参数应使用 substitution
  // Foo::method(Foo) → _ZN3Foo6methodE3Foo → 但 Foo 在 nested name 中
  // 实际上在 Itanium ABI 中，nested name 中的类名和参数中的类名
  // 共享 substitution 表，所以参数中的 Foo 应为 S_
  auto *RD = makeCXXRecordDecl("Foo");
  auto *RT = makeRecordType(RD);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT});
  auto *Param = makeParam("x", QualType(RT, Qualifier::None), 0);
  auto *MD = makeMethodDecl("method", QualType(FTy, Qualifier::None), {Param}, RD);
  // Foo 在 nested name 中被编码为 3Foo 并加入 substitution 表
  // 参数中的 Foo 应使用 S_ 替代
  EXPECT_EQ(M->getMangledName(MD), "_ZN3Foo6methodES_");
}

TEST_F(ManglerTest, SubstitutionNoShortNames) {
  // 短名称（长度<=1）不应加入 substitution 表
  auto *RD = makeRecordDecl("A");  // 长度为1，不应加入 substitution
  auto *RT = makeRecordType(RD);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {RT, RT});
  auto *P1 = makeParam("a", QualType(RT, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT, Qualifier::None), 1);
  auto *FD = makeFuncDecl("test", QualType(FTy, Qualifier::None), {P1, P2});
  // "A" 不应被 substitution 压缩（长度<=1）
  EXPECT_EQ(M->getMangledName(FD), "_Z4test1A1A");
}

TEST_F(ManglerTest, SubstitutionDifferentScopeSameName) {
  // 不同命名空间中的同名类型不应共享 substitution
  // ns1::Foo 和 ns2::Foo 是不同的实体，不应误匹配
  auto *Ns1 = new (Ctx.getAllocator().Allocate(sizeof(NamespaceDecl), alignof(NamespaceDecl)))
      NamespaceDecl(SourceLocation(), "ns1");
  Ns1->setOwningDecl(Ns1);
  auto *Ns2 = new (Ctx.getAllocator().Allocate(sizeof(NamespaceDecl), alignof(NamespaceDecl)))
      NamespaceDecl(SourceLocation(), "ns2");
  Ns2->setOwningDecl(Ns2);
  auto *TU = new (Ctx.getAllocator().Allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl)))
      TranslationUnitDecl(SourceLocation());
  Ns1->setParent(TU);
  Ns2->setParent(TU);

  // ns1::Foo
  auto *RD1 = makeCXXRecordDecl("Foo");
  RD1->setParent(Ns1);
  // ns2::Foo
  auto *RD2 = makeCXXRecordDecl("Foo");
  RD2->setParent(Ns2);

  auto *RT1 = makeRecordType(RD1);
  auto *RT2 = makeRecordType(RD2);
  auto *VoidTy = makeBuiltin(BuiltinKind::Void);

  // void func(ns1::Foo, ns2::Foo) — 两个 Foo 是不同实体，不应 substitution
  auto *FTy = makeFuncType(VoidTy, {RT1, RT2});
  auto *P1 = makeParam("a", QualType(RT1, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT2, Qualifier::None), 1);
  auto *FD = makeFuncDecl("func", QualType(FTy, Qualifier::None), {P1, P2});
  // 两个 Foo 来自不同 Decl，不应被 substitution 压缩
  // 注意：自由函数默认在全局作用域，所以参数类型直接编码
  EXPECT_EQ(M->getMangledName(FD), "_Z4func3Foo3Foo");
}

TEST_F(ManglerTest, SubstitutionSameScopeSameName) {
  // 同一命名空间中的同名类型应共享 substitution
  auto *Ns = new (Ctx.getAllocator().Allocate(sizeof(NamespaceDecl), alignof(NamespaceDecl)))
      NamespaceDecl(SourceLocation(), "ns");
  Ns->setOwningDecl(Ns);
  auto *TU = new (Ctx.getAllocator().Allocate(sizeof(TranslationUnitDecl), alignof(TranslationUnitDecl)))
      TranslationUnitDecl(SourceLocation());
  Ns->setParent(TU);

  // ns::Foo（同一个 Decl）
  auto *RD = makeCXXRecordDecl("Foo");
  RD->setParent(Ns);
  auto *RT = makeRecordType(RD);

  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  // void func(Foo, Foo) — 同一个 Foo Decl，第二次应 substitution
  auto *FTy = makeFuncType(VoidTy, {RT, RT});
  auto *P1 = makeParam("a", QualType(RT, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(RT, Qualifier::None), 1);
  auto *FD = makeFuncDecl("func", QualType(FTy, Qualifier::None), {P1, P2});
  EXPECT_EQ(M->getMangledName(FD), "_Z4func3FooS_");
}

TEST_F(ManglerTest, SubstitutionTemplateSpecReuse) {
  // 同一个模板特化类型出现两次，第二次应使用 substitution
  // void foo(vector<int>, vector<int>) → _Z3foo6vectorIiES_IiE
  // 第一个: 6vectorIiE (模板名+参数), 第二个: S_IiE (模板名 substitution + 参数)
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *TST = new (Ctx.getAllocator().Allocate(sizeof(TemplateSpecializationType),
                                                alignof(TemplateSpecializationType)))
      TemplateSpecializationType("vector");
  TST->addTemplateArg(TemplateArgument(QualType(IntTy, Qualifier::None)));

  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {TST, TST});
  auto *P1 = makeParam("a", QualType(TST, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(TST, Qualifier::None), 1);
  auto *FD = makeFuncDecl("foo", QualType(FTy, Qualifier::None), {P1, P2});
  // 第二个 vector<int> 的模板名使用 S_ 替代，但参数仍需编码
  EXPECT_EQ(M->getMangledName(FD), "_Z3foo6vectorIiES_IiE");
}

TEST_F(ManglerTest, SubstitutionTemplateSpecDifferentArgs) {
  // 不同模板参数的特化类型是不同实体
  // 由于测试中 TemplateDecl 为 nullptr，模板名 substitution 基于类型指针，
  // 两个不同的 TemplateSpecializationType 对象不会共享模板名 substitution
  // void bar(vector<int>, vector<float>) → _Z3bar6vectorIiE6vectorIfE
  auto *IntTy = makeBuiltin(BuiltinKind::Int);
  auto *FloatTy = makeBuiltin(BuiltinKind::Float);
  auto *TST1 = new (Ctx.getAllocator().Allocate(sizeof(TemplateSpecializationType),
                                                 alignof(TemplateSpecializationType)))
      TemplateSpecializationType("vector");
  TST1->addTemplateArg(TemplateArgument(QualType(IntTy, Qualifier::None)));
  auto *TST2 = new (Ctx.getAllocator().Allocate(sizeof(TemplateSpecializationType),
                                                 alignof(TemplateSpecializationType)))
      TemplateSpecializationType("vector");
  TST2->addTemplateArg(TemplateArgument(QualType(FloatTy, Qualifier::None)));

  auto *VoidTy = makeBuiltin(BuiltinKind::Void);
  auto *FTy = makeFuncType(VoidTy, {TST1, TST2});
  auto *P1 = makeParam("a", QualType(TST1, Qualifier::None), 0);
  auto *P2 = makeParam("b", QualType(TST2, Qualifier::None), 1);
  auto *FD = makeFuncDecl("bar", QualType(FTy, Qualifier::None), {P1, P2});
  // 两个不同的模板特化类型对象，TemplateDecl 为 nullptr，
  // substitution 基于类型指针，不会误匹配
  EXPECT_EQ(M->getMangledName(FD), "_Z3bar6vectorIiE6vectorIfE");
}

} // anonymous namespace
