//===--- CGCXX.h - C++ Specific Code Generation --------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the CGCXX class for C++ specific code generation
// (classes, constructors/destructors, virtual tables, inheritance).
//
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "blocktype/AST/Decl.h"

namespace llvm {
class GlobalVariable;
class ArrayType;
class Function;
class Value;
} // namespace llvm

namespace blocktype {

class CodeGenModule;

/// CGCXX — C++ 特有代码生成（类、构造/析构、虚函数表、继承）。
///
/// 职责（参照 Clang CGCXX + CGClass + CGVTables）：
/// 1. 生成类布局（字段偏移、大小、对齐、填充）
/// 2. 生成构造函数（基类初始化、成员初始化、构造函数体）
/// 3. 生成析构函数（成员析构、基类析构）
/// 4. 生成虚函数表（vtable 布局、vptr 初始化）
/// 5. 处理继承（单一/多重/虚继承）
class CGCXX {
  CodeGenModule &CGM;

  /// VTable 缓存：CXXRecordDecl → llvm::GlobalVariable
  llvm::DenseMap<const CXXRecordDecl *, llvm::GlobalVariable *> VTables;

  /// 字段偏移缓存：FieldDecl → 偏移量（字节）
  llvm::DenseMap<const FieldDecl *, uint64_t> FieldOffsetCache;

  /// 检查 CXXRecordDecl 是否有虚函数
  static bool hasVirtualFunctions(CXXRecordDecl *RD);

public:
  explicit CGCXX(CodeGenModule &M) : CGM(M) {}

  //===------------------------------------------------------------------===//
  // 类布局
  //===------------------------------------------------------------------===//

  /// 计算类的字段布局。返回每个字段的偏移量（字节）。
  llvm::SmallVector<uint64_t, 16> ComputeClassLayout(CXXRecordDecl *RD);

  /// 获取字段的偏移量。
  uint64_t GetFieldOffset(FieldDecl *FD);

  /// 获取类的完整大小。
  uint64_t GetClassSize(CXXRecordDecl *RD);

  //===------------------------------------------------------------------===//
  // 构造函数 / 析构函数
  //===------------------------------------------------------------------===//

  /// 生成构造函数的代码。
  void EmitConstructor(CXXConstructorDecl *Ctor, llvm::Function *Fn);

  /// 生成析构函数的代码。
  void EmitDestructor(CXXDestructorDecl *Dtor, llvm::Function *Fn);

  //===------------------------------------------------------------------===//
  // 虚函数表
  //===------------------------------------------------------------------===//

  /// 生成虚函数表。
  llvm::GlobalVariable *EmitVTable(CXXRecordDecl *RD);

  /// 获取虚函数表的类型。
  llvm::ArrayType *GetVTableType(CXXRecordDecl *RD);

  /// 计算虚函数表中方法的索引。
  unsigned GetVTableIndex(CXXMethodDecl *MD);

  /// 生成虚函数调用。
  llvm::Value *EmitVirtualCall(CXXMethodDecl *MD, llvm::Value *This,
                                llvm::ArrayRef<llvm::Value *> Args);

  //===------------------------------------------------------------------===//
  // 继承
  //===------------------------------------------------------------------===//

  /// 生成派生类到基类的偏移量。
  llvm::Value *EmitBaseOffset(llvm::Value *DerivedPtr, CXXRecordDecl *Base);

  /// 生成基类到派生类的偏移量。
  llvm::Value *EmitDerivedOffset(llvm::Value *BasePtr, CXXRecordDecl *Derived);

  //===------------------------------------------------------------------===//
  // 成员初始化
  //===------------------------------------------------------------------===//

  /// 生成基类初始化器。
  void EmitBaseInitializer(CXXRecordDecl *Class, llvm::Value *This,
                           CXXRecordDecl::BaseSpecifier *Base, Expr *Init);

  /// 生成成员初始化器。
  void EmitMemberInitializer(CXXRecordDecl *Class, llvm::Value *This,
                             FieldDecl *Field, Expr *Init);
};

} // namespace blocktype
