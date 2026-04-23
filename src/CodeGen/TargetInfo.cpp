//===--- TargetInfo.cpp - Target Platform Information ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/CodeGen/TargetInfo.h"
#include "llvm/TargetParser/Triple.h"

namespace blocktype {

//===----------------------------------------------------------------------===//
// 工厂方法
//===----------------------------------------------------------------------===//

std::unique_ptr<TargetInfo> TargetInfo::Create(llvm::StringRef TripleStr) {
  llvm::Triple T(TripleStr);

  if (T.getArch() == llvm::Triple::aarch64 ||
      T.getArch() == llvm::Triple::aarch64_be ||
      T.getArch() == llvm::Triple::aarch64_32) {
    return std::make_unique<AArch64TargetInfo>(TripleStr);
  }

  // Default: x86_64 for all other 64-bit architectures
  return std::make_unique<X86_64TargetInfo>(TripleStr);
}

//===----------------------------------------------------------------------===//
// TargetInfo 基类实现
//===----------------------------------------------------------------------===//

uint64_t TargetInfo::getTypeSize(QualType T) const {
  if (T.isNull()) return 0;
  const Type *Ty = T.getTypePtr();

  if (auto *BT = llvm::dyn_cast<BuiltinType>(Ty))
    return getBuiltinSize(BT->getKind());
  if (Ty->isPointerType())
    return getPointerSize();
  if (Ty->isReferenceType())
    return getPointerSize();
  if (auto *AT = llvm::dyn_cast<ArrayType>(Ty)) {
    uint64_t ElemSize = getTypeSize(QualType(AT->getElementType(), Qualifier::None));
    if (auto *CAT = llvm::dyn_cast<ConstantArrayType>(Ty))
      return ElemSize * CAT->getSize().getZExtValue();
    return ElemSize; // Incomplete/Variable array
  }
  // Records, enums, etc. — use pointer size as approximation
  return DL.getPointerSize(0);
}

uint64_t TargetInfo::getTypeAlign(QualType T) const {
  if (T.isNull()) return 1;
  const Type *Ty = T.getTypePtr();

  if (auto *BT = llvm::dyn_cast<BuiltinType>(Ty))
    return getBuiltinAlign(BT->getKind());
  if (Ty->isPointerType() || Ty->isReferenceType())
    return getPointerAlign();
  return 4; // Default alignment for unknown types
}

uint64_t TargetInfo::getBuiltinSize(BuiltinKind K) const {
  switch (K) {
  case BuiltinKind::Void:      return 0;
  case BuiltinKind::Bool:      return 1;
  case BuiltinKind::Char:
  case BuiltinKind::SignedChar:
  case BuiltinKind::UnsignedChar:
  case BuiltinKind::Char8:     return 1;
  case BuiltinKind::Short:
  case BuiltinKind::UnsignedShort:
  case BuiltinKind::Char16:    return 2;
  case BuiltinKind::Int:
  case BuiltinKind::UnsignedInt:
  case BuiltinKind::Float:
  case BuiltinKind::WChar:
  case BuiltinKind::Char32:    return 4;
  case BuiltinKind::Long:
  case BuiltinKind::UnsignedLong:
  case BuiltinKind::Double:    return 8;
  case BuiltinKind::LongLong:
  case BuiltinKind::UnsignedLongLong: return 8;
  case BuiltinKind::LongDouble:
    return getLongDoubleWidth();
  case BuiltinKind::Float128:  return 16;
  case BuiltinKind::Int128:
  case BuiltinKind::UnsignedInt128: return 16;
  case BuiltinKind::NullPtr:   return getPointerSize();
  default:                     return 0;
  }
}

uint64_t TargetInfo::getBuiltinAlign(BuiltinKind K) const {
  uint64_t Size = getBuiltinSize(K);
  if (Size == 0) return 1;
  if (Size == 1) return 1;
  if (Size <= 2) return 2;
  if (Size <= 4) return 4;
  if (Size <= 8) return 8;
  return 16;
}

bool TargetInfo::isStructReturnInRegister(QualType T) const {
  return getTypeSize(T) <= 16;
}

//===----------------------------------------------------------------------===//
// X86_64TargetInfo
//===----------------------------------------------------------------------===//

X86_64TargetInfo::X86_64TargetInfo(llvm::StringRef TripleStr)
    : TargetInfo(TripleStr,
                 "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"),
      IsDarwin(llvm::Triple(TripleStr).isOSDarwin()) {}

uint64_t X86_64TargetInfo::getBuiltinSize(BuiltinKind K) const {
  // x86_64: long double = 80-bit extended padded to 16 bytes
  if (K == BuiltinKind::LongDouble)
    return 16;
  return TargetInfo::getBuiltinSize(K);
}

bool X86_64TargetInfo::isStructReturnInRegister(QualType T) const {
  // System V AMD64 ABI: structs ≤ 16 bytes that can be classified as
  // INTEGER and/or SSE can be returned in registers (RAX/RDX or XMM0/XMM1).
  // Simplified: ≤ 16 bytes → in register.
  return getTypeSize(T) <= 16;
}

bool X86_64TargetInfo::isThisPassedInRegister() const {
  // System V AMD64: this pointer passed in RDI (register)
  return true;
}

//===----------------------------------------------------------------------===//
// AArch64TargetInfo
//===----------------------------------------------------------------------===//

AArch64TargetInfo::AArch64TargetInfo(llvm::StringRef TripleStr)
    : TargetInfo(TripleStr,
                 "e-m:o-i64:64-i128:128-n32:64-S128"),
      IsDarwin(llvm::Triple(TripleStr).isOSDarwin()) {}

uint64_t AArch64TargetInfo::getBuiltinSize(BuiltinKind K) const {
  // Darwin AArch64: long double = double (8 bytes)
  // Linux AArch64: long double = 16 bytes (IEEE quad)
  if (K == BuiltinKind::LongDouble)
    return IsDarwin ? 8 : 16;
  return TargetInfo::getBuiltinSize(K);
}

bool AArch64TargetInfo::isStructReturnInRegister(QualType T) const {
  // AAPCS64: structs ≤ 16 bytes that are not MEMORY class can be
  // returned in registers (x0/x1 or v0/v1).
  // Simplified: ≤ 16 bytes → in register.
  return getTypeSize(T) <= 16;
}

bool AArch64TargetInfo::isThisPassedInRegister() const {
  // AAPCS64: this pointer passed in X0 (register)
  return true;
}

uint64_t AArch64TargetInfo::getLongDoubleWidth() const {
  // Darwin AArch64: long double = double (8 bytes)
  // Linux AArch64: long double = IEEE quad (16 bytes)
  return IsDarwin ? 8 : 16;
}

} // namespace blocktype
