//===--- CGCXX.cpp - C++ Specific Code Generation --------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/CodeGen/CGCXX.h"
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/CodeGen/CodeGenTypes.h"
#include "blocktype/CodeGen/TargetInfo.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Casting.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// 辅助方法
//===----------------------------------------------------------------------===//

bool CGCXX::hasVirtualFunctions(CXXRecordDecl *RD) {
  if (!RD) return false;
  for (CXXMethodDecl *MD : RD->methods()) {
    if (MD->isVirtual()) return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// 类布局
//===----------------------------------------------------------------------===//

llvm::SmallVector<uint64_t, 16> CGCXX::ComputeClassLayout(CXXRecordDecl *RD) {
  llvm::SmallVector<uint64_t, 16> FieldOffsets;

  if (!RD) return FieldOffsets;

  uint64_t CurrentOffset = 0;

  // vptr 占位（如果有虚函数）
  if (hasVirtualFunctions(RD)) {
    uint64_t PtrSize = CGM.getTarget().getPointerSize();
    uint64_t PtrAlign = CGM.getTarget().getPointerAlign();
    CurrentOffset = llvm::alignTo(CurrentOffset, PtrAlign);
    FieldOffsets.push_back(CurrentOffset);
    CurrentOffset += PtrSize;
  }

  // 字段偏移
  for (FieldDecl *FD : RD->fields()) {
    uint64_t FieldSize = CGM.getTarget().getTypeSize(FD->getType());
    uint64_t FieldAlign = CGM.getTarget().getTypeAlign(FD->getType());

    // 对齐
    CurrentOffset = llvm::alignTo(CurrentOffset, FieldAlign);
    FieldOffsets.push_back(CurrentOffset);
    FieldOffsetCache[FD] = CurrentOffset;
    CurrentOffset += FieldSize;
  }

  return FieldOffsets;
}

uint64_t CGCXX::GetFieldOffset(FieldDecl *FD) {
  if (!FD) return 0;

  // 检查缓存
  auto It = FieldOffsetCache.find(FD);
  if (It != FieldOffsetCache.end()) return It->second;

  // 未计算过布局，回退到简化计算
  // 遍历所属 Record 的字段列表来计算偏移
  // 注意：FieldDecl 没有直接的父指针，需要调用方确保先调用了 ComputeClassLayout
  return 0;
}

uint64_t CGCXX::GetClassSize(CXXRecordDecl *RD) {
  if (!RD) return 0;

  auto Layout = ComputeClassLayout(RD);
  if (Layout.empty()) return 1; // 空类至少 1 字节

  // 找到最后一个字段的大小
  auto Fields = RD->fields();
  if (Fields.empty()) {
    // 只有 vptr 或完全空
    if (hasVirtualFunctions(RD)) {
      return CGM.getTarget().getPointerSize();
    }
    return 1; // 空类至少 1 字节
  }

  // 最后一个字段的末尾
  uint64_t LastOffset = Layout.back();
  uint64_t LastFieldSize = CGM.getTarget().getTypeSize(Fields.back()->getType());

  uint64_t Size = LastOffset + LastFieldSize;

  // 整体对齐
  uint64_t Align = 1;
  if (hasVirtualFunctions(RD)) {
    Align = std::max(Align, CGM.getTarget().getPointerAlign());
  }
  for (FieldDecl *F : RD->fields()) {
    Align = std::max(Align, CGM.getTarget().getTypeAlign(F->getType()));
  }

  Size = llvm::alignTo(Size, Align);

  return std::max(Size, (uint64_t)1); // 空类至少 1 字节
}

//===----------------------------------------------------------------------===//
// 构造函数 / 析构函数
//===----------------------------------------------------------------------===//

void CGCXX::EmitConstructor(CXXConstructorDecl *Ctor, llvm::Function *Fn) {
  // Stage 6.2 将实现完整的构造函数生成
  // 当前仅创建空函数体
  if (!Fn) return;
  if (!Fn->empty()) return;

  auto *Entry = llvm::BasicBlock::Create(CGM.getLLVMContext(), "entry", Fn);
  llvm::IRBuilder<> Builder(Entry);
  Builder.CreateRetVoid();
}

void CGCXX::EmitDestructor(CXXDestructorDecl *Dtor, llvm::Function *Fn) {
  // Stage 6.2 将实现完整的析构函数生成
  // 当前仅创建空函数体
  if (!Fn) return;
  if (!Fn->empty()) return;

  auto *Entry = llvm::BasicBlock::Create(CGM.getLLVMContext(), "entry", Fn);
  llvm::IRBuilder<> Builder(Entry);
  Builder.CreateRetVoid();
}

//===----------------------------------------------------------------------===//
// 虚函数表
//===----------------------------------------------------------------------===//

llvm::GlobalVariable *CGCXX::EmitVTable(CXXRecordDecl *RD) {
  if (!RD) return nullptr;

  // 检查缓存
  auto It = VTables.find(RD);
  if (It != VTables.end()) return It->second;

  // 收集虚函数
  llvm::SmallVector<llvm::Constant *, 8> VTableEntries;

  // RTTI 占位（typeinfo 指针）
  VTableEntries.push_back(
      llvm::ConstantPointerNull::get(
          llvm::PointerType::get(CGM.getLLVMContext(), 0)));

  // 收集虚方法
  for (CXXMethodDecl *MD : RD->methods()) {
    if (!MD->isVirtual()) continue;
    llvm::Function *Fn = CGM.GetOrCreateFunctionDecl(MD);
    if (Fn) {
      VTableEntries.push_back(Fn);
    } else {
      VTableEntries.push_back(
          llvm::ConstantPointerNull::get(
              llvm::PointerType::get(CGM.getLLVMContext(), 0)));
    }
  }

  // 创建 vtable 类型
  auto *VTableTy = llvm::ArrayType::get(
      llvm::PointerType::get(CGM.getLLVMContext(), 0),
      VTableEntries.size());

  auto *VTableInit = llvm::ConstantArray::get(VTableTy, VTableEntries);

  // 创建全局变量
  auto *GV = new llvm::GlobalVariable(
      *CGM.getModule(), VTableTy, true,
      llvm::GlobalValue::ExternalLinkage, VTableInit,
      "_ZTV" + RD->getName());

  VTables[RD] = GV;
  return GV;
}

llvm::ArrayType *CGCXX::GetVTableType(CXXRecordDecl *RD) {
  if (!RD) return nullptr;

  unsigned NumEntries = 1; // RTTI 指针
  for (CXXMethodDecl *MD : RD->methods()) {
    if (MD->isVirtual()) ++NumEntries;
  }

  return llvm::ArrayType::get(
      llvm::PointerType::get(CGM.getLLVMContext(), 0), NumEntries);
}

unsigned CGCXX::GetVTableIndex(CXXMethodDecl *MD) {
  if (!MD) return 0;

  CXXRecordDecl *RD = MD->getParent();
  if (!RD) return 0;

  unsigned Idx = 1; // 跳过 RTTI 指针
  for (CXXMethodDecl *M : RD->methods()) {
    if (M == MD) return Idx;
    if (M->isVirtual()) ++Idx;
  }

  return 0;
}

llvm::Value *CGCXX::EmitVirtualCall(CXXMethodDecl *MD, llvm::Value *This,
                                     llvm::ArrayRef<llvm::Value *> Args) {
  // Stage 6.2 将实现完整的虚函数调用
  (void)MD;
  (void)This;
  (void)Args;
  return nullptr;
}

//===----------------------------------------------------------------------===//
// 继承
//===----------------------------------------------------------------------===//

llvm::Value *CGCXX::EmitBaseOffset(llvm::Value *DerivedPtr, CXXRecordDecl *Base) {
  // Stage 6.3 将实现完整的继承偏移计算
  (void)DerivedPtr;
  (void)Base;
  return nullptr;
}

llvm::Value *CGCXX::EmitDerivedOffset(llvm::Value *BasePtr, CXXRecordDecl *Derived) {
  // Stage 6.3 将实现完整的继承偏移计算
  (void)BasePtr;
  (void)Derived;
  return nullptr;
}

//===----------------------------------------------------------------------===//
// 成员初始化
//===----------------------------------------------------------------------===//

void CGCXX::EmitBaseInitializer(CXXRecordDecl *Class, llvm::Value *This,
                                CXXRecordDecl::BaseSpecifier *Base, Expr *Init) {
  // Stage 6.2 将实现完整的基类初始化
  (void)Class;
  (void)This;
  (void)Base;
  (void)Init;
}

void CGCXX::EmitMemberInitializer(CXXRecordDecl *Class, llvm::Value *This,
                                  FieldDecl *Field, Expr *Init) {
  // Stage 6.2 将实现完整的成员初始化
  (void)Class;
  (void)This;
  (void)Field;
  (void)Init;
}
