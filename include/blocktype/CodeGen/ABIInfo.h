//===--- ABIInfo.h - ABI Classification Interface ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the ABIInfo class hierarchy for classifying function
// argument and return value passing conventions according to platform ABIs.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/CodeGen/CodeGenTypes.h"
#include "blocktype/CodeGen/TargetInfo.h"
#include <memory>

namespace blocktype {

//===----------------------------------------------------------------------===//
// ABIClass — x86_64 System V AMD64 ABI register classes
//===----------------------------------------------------------------------===//

/// ABIClass — Classification of a type or struct field under the
/// System V AMD64 ABI. Used by X86_64ABIInfo to determine whether
/// a value is passed in integer registers, SSE registers, or memory.
enum class ABIClass {
  NoClass,   ///< Initial / unclassified
  Integer,   ///< Passed in general-purpose register (rax/rdi/rsi/rcx/rdx/r8-r15)
  SSE,       ///< Passed in SSE register (xmm0-xmm15)
  SSEUp,     ///< Upper half of an SSE value (follows SSE in struct merge)
  X87,       ///< Passed on x87 FP stack (st0)
  X87Up,     ///< Upper half of x87 (follows X87)
  ComplexX87,///< Complex long double on x87
  Memory     ///< Passed in memory (via hidden pointer / sret)
};

//===----------------------------------------------------------------------===//
// AArch64ABIPrimitiveClass — AAPCS64 primitive type classes
//===----------------------------------------------------------------------===//

/// AArch64ABIPrimitiveClass — Classification of primitive types under
/// the AAPCS64 ABI. Used by AArch64ABIInfo for member classification.
enum class AArch64ABIPrimitiveClass {
  Integer,   ///< General-purpose register (x0-x7 / w0-w7)
  FP,        ///< Floating-point register (s0-s31 / d0-d31)
  Vector,    ///< SIMD/FP register (v0-v31 / b0-h0-s0-d0-q0)
  Memory     ///< Passed in memory
};

//===----------------------------------------------------------------------===//
// ABIInfo — Abstract base class for ABI classification
//===----------------------------------------------------------------------===//

/// ABIInfo — Abstract base class for platform-specific ABI classification.
///
/// Subclasses implement classifyReturnType() and classifyArgumentType()
/// according to their respective ABI specifications:
/// - X86_64ABIInfo: System V AMD64 ABI
/// - AArch64ABIInfo: AAPCS64 ABI
///
/// The classification results are ABIArgInfo structs, which describe
/// how each parameter or return value should be passed at the LLVM IR level.
class ABIInfo {
protected:
  const TargetInfo &Target;

  explicit ABIInfo(const TargetInfo &T) : Target(T) {}

  /// Compute the size of a record type by summing field sizes.
  /// This is needed because TargetInfo::getTypeSize() returns pointer size
  /// as an approximation for record types.
  uint64_t computeRecordSize(QualType Ty) const;

public:
  virtual ~ABIInfo() = default;

  /// Classify a return type according to the platform ABI.
  virtual ABIArgInfo classifyReturnType(QualType RetTy) const = 0;

  /// Classify an argument type according to the platform ABI.
  virtual ABIArgInfo classifyArgumentType(QualType ParamTy) const = 0;

  /// Factory: create the appropriate ABIInfo subclass for the target.
  static std::unique_ptr<ABIInfo> Create(const TargetInfo &T);
};

//===----------------------------------------------------------------------===//
// X86_64ABIInfo — System V AMD64 ABI classification
//===----------------------------------------------------------------------===//

/// X86_64ABIInfo — Implements the System V AMD64 ABI classification algorithm.
///
/// Key rules (simplified for BlockType):
/// 1. Primitive types: INTEGER (int/pointer) or SSE (float/double)
/// 2. Structures: recursively classify each field, then merge results
///    - If final merged class is MEMORY → pass via sret
///    - If ≤16 bytes and non-MEMORY → pass in registers (Direct or Expand)
/// 3. __int128: passed as two INTEGER registers
/// 4. long double (80-bit): passed on x87 stack or via memory
class X86_64ABIInfo : public ABIInfo {
public:
  explicit X86_64ABIInfo(const TargetInfo &T) : ABIInfo(T) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const override;
  ABIArgInfo classifyArgumentType(QualType ParamTy) const override;

  //===------------------------------------------------------------------===//
  // System V AMD64 classification helpers
  //===------------------------------------------------------------------===//

  /// Classify a single primitive type.
  ABIClass classifyPrimitive(QualType Ty) const;

  /// Recursively classify a record type by merging field classifications.
  /// Returns the merged classification result (Lo8, Hi8 pair).
  void classifyRecord(QualType Ty, ABIClass &Lo, ABIClass &Hi) const;

  /// Merge two classifications according to the System V algorithm.
  static ABIClass mergeClass(ABIClass Cumulative, ABIClass Field);

  /// Post-merge cleanup: simplify classification pairs.
  static void postMerge(ABIClass &Lo, ABIClass &Hi);

  /// Determine the final ABIArgInfo from a (Lo, Hi) classification pair.
  ABIArgInfo classifyBasedOnClasses(ABIClass Lo, ABIClass Hi,
                                    uint64_t Size, QualType Ty) const;
};

//===----------------------------------------------------------------------===//
// AArch64ABIInfo — AAPCS64 ABI classification
//===----------------------------------------------------------------------===//

/// AArch64ABIInfo — Implements the AAPCS64 ABI classification algorithm.
///
/// Key rules (simplified for BlockType):
/// 1. Primitive types: INTEGER, FP, or Vector
/// 2. HFA/HVA: Homogeneous Floating-point/Vector Aggregate
///    - 2-4 identical FP/vector members → passed in FP/SIMD registers
/// 3. Structures ≤16 bytes: classified by members, may use up to 2 registers
/// 4. Structures >16 bytes or MEMORY class → passed via sret
class AArch64ABIInfo : public ABIInfo {
public:
  explicit AArch64ABIInfo(const TargetInfo &T) : ABIInfo(T) {}

  ABIArgInfo classifyReturnType(QualType RetTy) const override;
  ABIArgInfo classifyArgumentType(QualType ParamTy) const override;

  //===------------------------------------------------------------------===//
  // AAPCS64 classification helpers
  //===------------------------------------------------------------------===//

  /// Classify a single primitive type.
  AArch64ABIPrimitiveClass classifyPrimitive(QualType Ty) const;

  /// Check if a record type is an HFA (Homogeneous Floating-point Aggregate).
  /// Returns true if the record has 2-4 identical FP members.
  bool isHFA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
             unsigned &MemberCount) const;

  /// Check if a record type is an HVA (Homogeneous Vector Aggregate).
  /// Returns true if the record has 2-4 identical Vector members.
  bool isHVA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
             unsigned &MemberCount) const;

  /// Check if a record type is an HFA or HVA (combined).
  bool isHFPA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
              unsigned &MemberCount) const;

  /// Classify a record type by examining its members.
  AArch64ABIPrimitiveClass classifyRecord(QualType Ty) const;
};

} // namespace blocktype