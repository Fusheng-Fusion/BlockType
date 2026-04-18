//===--- TargetInfo.h - Target Platform Information ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the TargetInfo class for target platform information.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/IR/DataLayout.h"
#include "llvm/ADT/StringRef.h"
#include "blocktype/AST/Type.h"

namespace blocktype {

/// TargetInfo — 目标平台类型信息。
///
/// 职责（参照 Clang TargetInfo）：
/// 1. 提供类型大小和对齐信息
/// 2. 管理 LLVM DataLayout
/// 3. 提供目标三元组信息
/// 4. 定义 ABI 特定规则
class TargetInfo {
  llvm::DataLayout DL;
  std::string TripleStr;

public:
  explicit TargetInfo(llvm::StringRef TargetTriple);

  //===------------------------------------------------------------------===//
  // 类型大小和对齐
  //===------------------------------------------------------------------===//

  /// 获取类型的大小（字节）。
  uint64_t getTypeSize(QualType T) const;

  /// 获取类型的对齐（字节）。
  uint64_t getTypeAlign(QualType T) const;

  /// 获取内建类型的大小。
  uint64_t getBuiltinSize(BuiltinKind K) const;

  /// 获取内建类型的对齐。
  uint64_t getBuiltinAlign(BuiltinKind K) const;

  /// 获取指针大小。
  uint64_t getPointerSize() const { return DL.getPointerSize(0); }

  /// 获取指针对齐。
  uint64_t getPointerAlign() const { return DL.getPointerABIAlignment(0).value(); }

  //===------------------------------------------------------------------===//
  // DataLayout 访问
  //===------------------------------------------------------------------===//

  const llvm::DataLayout &getDataLayout() const { return DL; }
  llvm::StringRef getTriple() const { return TripleStr; }

  //===------------------------------------------------------------------===//
  // ABI 查询
  //===------------------------------------------------------------------===//

  /// 结构体是否通过寄存器返回（而非内存）。
  bool isStructReturnInRegister(QualType T) const;

  /// this 指针是否通过寄存器传递。
  bool isThisPassedInRegister() const { return true; }

  /// 枚举类型的底层整数大小（字节）。
  uint64_t getEnumSize() const { return 4; }
};

} // namespace blocktype
