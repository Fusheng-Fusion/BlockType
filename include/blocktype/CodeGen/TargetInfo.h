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
#include <memory>

namespace blocktype {

/// TargetInfo — 目标平台类型信息（抽象基类）。
///
/// 职责（参照 Clang TargetInfo）：
/// 1. 提供类型大小和对齐信息
/// 2. 管理 LLVM DataLayout
/// 3. 提供目标三元组信息
/// 4. 定义 ABI 特定规则
///
/// 子类：X86_64TargetInfo、AArch64TargetInfo
class TargetInfo {
protected:
  llvm::DataLayout DL;
  std::string TripleStr;

  TargetInfo(llvm::StringRef TargetTriple, llvm::StringRef DataLayoutStr)
      : DL(DataLayoutStr), TripleStr(TargetTriple.str()) {}

public:
  virtual ~TargetInfo() = default;

  // Non-copyable
  TargetInfo(const TargetInfo &) = delete;
  TargetInfo &operator=(const TargetInfo &) = delete;

  //===------------------------------------------------------------------===//
  // 工厂方法
  //===------------------------------------------------------------------===//

  /// 根据目标三元组创建对应的 TargetInfo 子类实例。
  static std::unique_ptr<TargetInfo> Create(llvm::StringRef Triple);

  //===------------------------------------------------------------------===//
  // 类型大小和对齐
  //===------------------------------------------------------------------===//

  /// 获取类型的大小（字节）。
  uint64_t getTypeSize(QualType T) const;

  /// 获取类型的对齐（字节）。
  uint64_t getTypeAlign(QualType T) const;

  /// 获取内建类型的大小。
  virtual uint64_t getBuiltinSize(BuiltinKind K) const;

  /// 获取内建类型的对齐。
  virtual uint64_t getBuiltinAlign(BuiltinKind K) const;

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
  virtual bool isStructReturnInRegister(QualType T) const;

  /// this 指针是否通过寄存器传递。
  virtual bool isThisPassedInRegister() const { return true; }

  /// 枚举类型的底层整数大小（字节）。
  virtual uint64_t getEnumSize() const { return 4; }

  //===------------------------------------------------------------------===//
  // 平台查询
  //===------------------------------------------------------------------===//

  virtual bool isLinux() const { return false; }
  virtual bool isMacOS() const { return false; }
  virtual bool isX86_64() const { return false; }
  virtual bool isAArch64() const { return false; }

  //===------------------------------------------------------------------===//
  // 平台特定类型信息
  //===------------------------------------------------------------------===//

  /// long 类型宽度（字节）。
  virtual uint64_t getLongWidth() const { return 8; }

  /// long double 类型宽度（字节）。
  virtual uint64_t getLongDoubleWidth() const { return 16; }

  /// 最大向量对齐（字节）。
  virtual uint64_t getMaxVectorAlign() const { return 16; }
};

/// X86_64TargetInfo — Linux x86_64 / System V AMD64 ABI 目标信息。
class X86_64TargetInfo : public TargetInfo {
  bool IsDarwin;

public:
  explicit X86_64TargetInfo(llvm::StringRef Triple);

  uint64_t getBuiltinSize(BuiltinKind K) const override;
  bool isStructReturnInRegister(QualType T) const override;
  bool isThisPassedInRegister() const override;

  bool isLinux() const override { return !IsDarwin; }
  bool isMacOS() const override { return IsDarwin; }
  bool isX86_64() const override { return true; }

  uint64_t getLongWidth() const override { return 8; }
  uint64_t getLongDoubleWidth() const override { return 16; }
  uint64_t getMaxVectorAlign() const override { return 16; }
};

/// AArch64TargetInfo — macOS ARM64 / AAPCS64 ABI 目标信息。
class AArch64TargetInfo : public TargetInfo {
  bool IsDarwin;

public:
  explicit AArch64TargetInfo(llvm::StringRef Triple);

  uint64_t getBuiltinSize(BuiltinKind K) const override;
  bool isStructReturnInRegister(QualType T) const override;
  bool isThisPassedInRegister() const override;

  bool isLinux() const override { return !IsDarwin; }
  bool isMacOS() const override { return IsDarwin; }
  bool isAArch64() const override { return true; }

  uint64_t getLongWidth() const override { return 8; }
  uint64_t getLongDoubleWidth() const override;
  uint64_t getMaxVectorAlign() const override { return 16; }
};

} // namespace blocktype
