//===--- CodeGenConstant.h - Constant Expression CodeGen -----*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the CodeGenConstant class for generating LLVM constants
// from AST constant expressions.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/Constants.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/APFloat.h"
#include "blocktype/AST/Type.h"

namespace blocktype {

class CodeGenModule;
class IntegerLiteral;
class FloatingLiteral;
class StringLiteral;
class CharacterLiteral;
class InitListExpr;

/// CodeGenConstant — 常量表达式到 LLVM 常量的生成器。
///
/// 职责（参照 Clang ConstantEmitter）：
/// 1. 将 AST 常量表达式转换为 llvm::Constant
/// 2. 处理整数字面量、浮点字面量、字符串字面量
/// 3. 处理空指针常量、布尔常量
/// 4. 处理聚合常量（数组初始化列表、结构体初始化列表）
class CodeGenConstant {
  CodeGenModule &CGM;

public:
  explicit CodeGenConstant(CodeGenModule &M) : CGM(M) {}

  //===------------------------------------------------------------------===//
  // 常量生成主接口
  //===------------------------------------------------------------------===//

  /// 将表达式作为常量发射。用于全局变量初始化器、枚举值等。
  llvm::Constant *EmitConstant(Expr *E);

  /// 将表达式作为常量发射，指定目标类型。
  llvm::Constant *EmitConstantForType(Expr *E, QualType DestType);

  //===------------------------------------------------------------------===//
  // 字面量常量
  //===------------------------------------------------------------------===//

  /// 整数字面量 → llvm::ConstantInt
  llvm::Constant *EmitIntLiteral(IntegerLiteral *IL);

  /// 浮点字面量 → llvm::ConstantFP
  llvm::Constant *EmitFloatLiteral(FloatingLiteral *FL);

  /// 字符串字面量 → llvm::GlobalVariable (llvm::ConstantDataArray)
  llvm::Constant *EmitStringLiteral(StringLiteral *SL);

  /// 布尔字面量 → llvm::ConstantInt(i1)
  llvm::Constant *EmitBoolLiteral(bool Value);

  /// 字符字面量 → llvm::ConstantInt(i32)
  llvm::Constant *EmitCharLiteral(CharacterLiteral *CL);

  /// nullptr 字面量 → llvm::ConstantPointerNull
  llvm::Constant *EmitNullPointer(QualType T);

  //===------------------------------------------------------------------===//
  // 聚合常量
  //===------------------------------------------------------------------===//

  /// 初始化列表 → llvm::ConstantStruct 或 llvm::ConstantArray
  llvm::Constant *EmitInitListExpr(InitListExpr *ILE);

  //===------------------------------------------------------------------===//
  // 零值 / 特殊常量
  //===------------------------------------------------------------------===//

  /// 发射类型的零值（用于默认初始化）。
  llvm::Constant *EmitZeroValue(QualType T);

  /// 发射类型的未定义值。
  llvm::Constant *EmitUndefValue(QualType T);

  //===------------------------------------------------------------------===//
  // 类型转换常量
  //===------------------------------------------------------------------===//

  /// 整数类型之间的静态转换常量。
  llvm::Constant *EmitIntCast(llvm::Constant *C, QualType From, QualType To);

  /// 浮点到整数的转换常量。
  llvm::Constant *EmitFloatToIntCast(llvm::Constant *C, QualType From, QualType To);

  /// 整数到浮点的转换常量。
  llvm::Constant *EmitIntToFloatCast(llvm::Constant *C, QualType From, QualType To);

  //===------------------------------------------------------------------===//
  // 工具方法
  //===------------------------------------------------------------------===//

  /// 获取类型的空指针常量。
  llvm::ConstantPointerNull *GetNullPointer(QualType T);

  /// 获取整数类型的零值。
  llvm::ConstantInt *GetIntZero(QualType T);
  llvm::ConstantInt *GetIntOne(QualType T);

  /// 获取 LLVM 上下文。
  llvm::LLVMContext &getLLVMContext() const;
};

} // namespace blocktype
