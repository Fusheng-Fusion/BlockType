//===--- ABIInfo.cpp - ABI Classification Implementation ----*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/CodeGen/ABIInfo.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Type.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// ABIInfo Factory
//===----------------------------------------------------------------------===//

std::unique_ptr<ABIInfo> ABIInfo::Create(const TargetInfo &T) {
  if (T.isAArch64())
    return std::make_unique<AArch64ABIInfo>(T);
  // Default: x86_64 (System V AMD64 ABI)
  return std::make_unique<X86_64ABIInfo>(T);
}

uint64_t ABIInfo::computeRecordSize(QualType Ty) const {
  if (Ty.isNull()) return 0;
  const Type *T = Ty.getTypePtr();
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (!RT) return Target.getTypeSize(Ty);

  RecordDecl *RD = RT->getDecl();
  if (!RD) return 0;

  uint64_t Size = 0;
  for (FieldDecl *Field : RD->fields()) {
    Size += Target.getTypeSize(Field->getType());
  }
  return Size;
}

//===----------------------------------------------------------------------===//
// X86_64ABIInfo — System V AMD64 ABI Classification
//===----------------------------------------------------------------------===//

ABIClass X86_64ABIInfo::classifyPrimitive(QualType Ty) const {
  if (Ty.isNull()) return ABIClass::NoClass;
  const Type *T = Ty.getTypePtr();

  // Void: no classification
  if (T->isVoidType()) return ABIClass::NoClass;

  // Integer types → INTEGER
  if (T->isIntegerType() || T->isBooleanType()) return ABIClass::Integer;

  // Enum types → INTEGER (underlying integer)
  if (T->isEnumType()) return ABIClass::Integer;

  // Pointer / reference types → INTEGER
  if (T->isPointerType() || T->isReferenceType()) return ABIClass::Integer;

  // Floating-point types
  if (auto *BT = llvm::dyn_cast<BuiltinType>(T)) {
    switch (BT->getKind()) {
    case BuiltinKind::Float:
    case BuiltinKind::Double:
      return ABIClass::SSE;
    case BuiltinKind::LongDouble:
      // x86_64: long double is 80-bit extended → X87 class
      // But for simplicity, if size ≤ 16 bytes we treat as SSE for now
      // Full implementation would use X87/X87Up
      return ABIClass::SSE;
    case BuiltinKind::Float128:
      return ABIClass::SSE;
    default:
      break;
    }
  }

  return ABIClass::NoClass;
}

ABIClass X86_64ABIInfo::mergeClass(ABIClass Cumulative, ABIClass Field) {
  // System V AMD64 merge algorithm (Table 3.6):
  // If either is MEMORY → MEMORY
  if (Cumulative == ABIClass::Memory || Field == ABIClass::Memory)
    return ABIClass::Memory;

  // If either is X87 or X87Up → Memory
  if (Cumulative == ABIClass::X87 || Field == ABIClass::X87 ||
      Cumulative == ABIClass::X87Up || Field == ABIClass::X87Up)
    return ABIClass::Memory;

  // SSEUp + SSE → SSE (SSEUp follows SSE)
  if (Cumulative == ABIClass::SSEUp && Field == ABIClass::SSE)
    return ABIClass::SSE;
  if (Field == ABIClass::SSEUp && Cumulative == ABIClass::SSE)
    return ABIClass::SSE;

  // SSEUp + anything else → the other class (SSEUp is subordinate)
  if (Cumulative == ABIClass::SSEUp) return Field;
  if (Field == ABIClass::SSEUp) return Cumulative;

  // Integer + anything → Integer
  if (Cumulative == ABIClass::Integer || Field == ABIClass::Integer)
    return ABIClass::Integer;

  // SSE + SSE → SSE
  if (Cumulative == ABIClass::SSE && Field == ABIClass::SSE)
    return ABIClass::SSE;

  // NoClass + anything → the other class
  if (Cumulative == ABIClass::NoClass) return Field;
  if (Field == ABIClass::NoClass) return Cumulative;

  // ComplexX87 → Memory
  if (Cumulative == ABIClass::ComplexX87 || Field == ABIClass::ComplexX87)
    return ABIClass::Memory;

  return ABIClass::Memory;
}

void X86_64ABIInfo::postMerge(ABIClass &Lo, ABIClass &Hi) {
  // Post-merge cleanup per System V AMD64 ABI:
  // If Hi is NoClass or SSEUp, it's absorbed into Lo
  if (Hi == ABIClass::NoClass || Hi == ABIClass::SSEUp)
    Hi = ABIClass::NoClass;

  // If Lo is NoClass, promote Hi
  if (Lo == ABIClass::NoClass && Hi != ABIClass::NoClass) {
    Lo = Hi;
    Hi = ABIClass::NoClass;
  }
}

void X86_64ABIInfo::classifyRecord(QualType Ty, ABIClass &Lo, ABIClass &Hi) const {
  Lo = ABIClass::NoClass;
  Hi = ABIClass::NoClass;

  if (Ty.isNull()) return;
  const Type *T = Ty.getTypePtr();
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (!RT) return;

  RecordDecl *RD = RT->getDecl();
  if (!RD) return;

  uint64_t Offset = 0;
  const uint64_t PtrSize = Target.getPointerSize(); // 8 bytes on x86_64

  for (FieldDecl *Field : RD->fields()) {
    QualType FT = Field->getType();
    uint64_t FieldSize = Target.getTypeSize(FT);

    // Classify the field
    ABIClass FieldLo, FieldHi;

    if (FT->isRecordType()) {
      // Nested record: recursive classification
      classifyRecord(FT, FieldLo, FieldHi);
    } else {
      // Primitive type
      FieldLo = classifyPrimitive(FT);
      FieldHi = ABIClass::NoClass;

      // __int128: occupies two 8-byte slots → two INTEGER classifications
      if (auto *BT = llvm::dyn_cast<BuiltinType>(FT.getTypePtr())) {
        if (BT->getKind() == BuiltinKind::Int128 ||
            BT->getKind() == BuiltinKind::UnsignedInt128) {
          FieldLo = ABIClass::Integer;
          FieldHi = ABIClass::Integer;
        }
      }
    }

    // Determine which 8-byte half this field falls into
    uint64_t FieldStart = Offset;
    uint64_t FieldEnd = Offset + FieldSize;

    // Merge into the appropriate half(es)
    // A field can span both halves (e.g., __int128 at offset 0)
    if (FieldStart < 8) {
      // Field starts in Lo half
      Lo = mergeClass(Lo, FieldLo);
      if (FieldEnd > 8) {
        // Field spans into Hi half
        Hi = mergeClass(Hi, FieldHi);
      }
    } else if (FieldStart < 16) {
      // Field starts in Hi half — its Lo classification goes into Hi
      Hi = mergeClass(Hi, FieldLo);
    }

    // Advance offset (simplified: use field size, no alignment padding)
    // A full implementation would use DataLayout for proper offsets
    Offset += FieldSize;
  }

  // Post-merge cleanup
  postMerge(Lo, Hi);
}

ABIArgInfo X86_64ABIInfo::classifyBasedOnClasses(ABIClass Lo, ABIClass Hi,
                                                  uint64_t Size,
                                                  QualType Ty) const {
  // If either half is MEMORY → pass via sret
  if (Lo == ABIClass::Memory || Hi == ABIClass::Memory)
    return ABIArgInfo::getSRet(nullptr);

  // If > 16 bytes and not MEMORY → still memory (exceeds register capacity)
  if (Size > 16)
    return ABIArgInfo::getSRet(nullptr);

  // If both NoClass → shouldn't happen for valid types, but handle gracefully
  if (Lo == ABIClass::NoClass && Hi == ABIClass::NoClass)
    return ABIArgInfo::getDirect();

  // INTEGER class → Direct
  if (Lo == ABIClass::Integer && Hi == ABIClass::NoClass)
    return ABIArgInfo::getDirect();

  // SSE class → Direct
  if (Lo == ABIClass::SSE && Hi == ABIClass::NoClass)
    return ABIArgInfo::getDirect();

  // Two INTEGER halves (e.g., __int128 or struct with two int fields)
  if (Lo == ABIClass::Integer && Hi == ABIClass::Integer)
    return ABIArgInfo::getExpand();

  // INTEGER + SSE (e.g., struct { int; double })
  if ((Lo == ABIClass::Integer && Hi == ABIClass::SSE) ||
      (Lo == ABIClass::SSE && Hi == ABIClass::Integer))
    return ABIArgInfo::getExpand();

  // Two SSE halves (e.g., struct { double; double })
  if (Lo == ABIClass::SSE && Hi == ABIClass::SSE)
    return ABIArgInfo::getExpand();

  // Default: Direct
  return ABIArgInfo::getDirect();
}

ABIArgInfo X86_64ABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy.isNull()) return ABIArgInfo::getDirect();

  const Type *T = RetTy.getTypePtr();

  // Void → Direct (no return value)
  if (T->isVoidType()) return ABIArgInfo::getDirect();

  // Non-record types: classify as primitive
  if (!T->isRecordType()) {
    ABIClass Cls = classifyPrimitive(RetTy);
    if (Cls == ABIClass::Integer)
      return ABIArgInfo::getDirect();
    if (Cls == ABIClass::SSE)
      return ABIArgInfo::getDirect();
    return ABIArgInfo::getDirect();
  }

  // Record types: full classification
  uint64_t Size = computeRecordSize(RetTy);

  // > 16 bytes → sret
  if (Size > 16)
    return ABIArgInfo::getSRet(nullptr);

  ABIClass Lo, Hi;
  classifyRecord(RetTy, Lo, Hi);

  // If MEMORY class → sret
  if (Lo == ABIClass::Memory || Hi == ABIClass::Memory)
    return ABIArgInfo::getSRet(nullptr);

  // Classify based on register classes
  return classifyBasedOnClasses(Lo, Hi, Size, RetTy);
}

ABIArgInfo X86_64ABIInfo::classifyArgumentType(QualType ParamTy) const {
  if (ParamTy.isNull()) return ABIArgInfo::getDirect();

  const Type *T = ParamTy.getTypePtr();

  // Non-record types: classify as primitive
  if (!T->isRecordType()) {
    ABIClass Cls = classifyPrimitive(ParamTy);
    if (Cls == ABIClass::Integer)
      return ABIArgInfo::getDirect();
    if (Cls == ABIClass::SSE)
      return ABIArgInfo::getDirect();
    return ABIArgInfo::getDirect();
  }

  // Record types: full classification
  uint64_t Size = computeRecordSize(ParamTy);

  // > 16 bytes → memory (by value, passed as pointer)
  if (Size > 16)
    return ABIArgInfo::getDirect();

  ABIClass Lo, Hi;
  classifyRecord(ParamTy, Lo, Hi);

  // If MEMORY class → pass by value (will be copied to stack)
  if (Lo == ABIClass::Memory || Hi == ABIClass::Memory)
    return ABIArgInfo::getDirect();

  // Classify based on register classes
  return classifyBasedOnClasses(Lo, Hi, Size, ParamTy);
}

//===----------------------------------------------------------------------===//
// AArch64ABIInfo — AAPCS64 ABI Classification
//===----------------------------------------------------------------------===//

AArch64ABIPrimitiveClass AArch64ABIInfo::classifyPrimitive(QualType Ty) const {
  if (Ty.isNull()) return AArch64ABIPrimitiveClass::Integer;
  const Type *T = Ty.getTypePtr();

  // Void: no classification
  if (T->isVoidType()) return AArch64ABIPrimitiveClass::Integer;

  // Integer types → INTEGER
  if (T->isIntegerType() || T->isBooleanType())
    return AArch64ABIPrimitiveClass::Integer;

  // Enum types → INTEGER
  if (T->isEnumType()) return AArch64ABIPrimitiveClass::Integer;

  // Pointer / reference types → INTEGER
  if (T->isPointerType() || T->isReferenceType())
    return AArch64ABIPrimitiveClass::Integer;

  // Floating-point types → FP
  if (auto *BT = llvm::dyn_cast<BuiltinType>(T)) {
    switch (BT->getKind()) {
    case BuiltinKind::Float:
    case BuiltinKind::Double:
    case BuiltinKind::LongDouble:
    case BuiltinKind::Float128:
      return AArch64ABIPrimitiveClass::FP;
    default:
      break;
    }
  }

  return AArch64ABIPrimitiveClass::Integer;
}

bool AArch64ABIInfo::isHFA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
                            unsigned &MemberCount) const {
  MemberCount = 0;
  MemberClass = AArch64ABIPrimitiveClass::Integer;

  if (Ty.isNull()) return false;
  const Type *T = Ty.getTypePtr();
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (!RT) return false;

  RecordDecl *RD = RT->getDecl();
  if (!RD) return false;

  AArch64ABIPrimitiveClass FirstClass = AArch64ABIPrimitiveClass::Integer;
  unsigned Count = 0;

  for (FieldDecl *Field : RD->fields()) {
    QualType FT = Field->getType();

    // Skip non-FP fields
    AArch64ABIPrimitiveClass FC = classifyPrimitive(FT);
    if (FC != AArch64ABIPrimitiveClass::FP)
      return false;

    // All members must be the same FP type
    if (Count == 0) {
      FirstClass = FC;
    } else if (FC != FirstClass) {
      return false;
    }

    ++Count;
  }

  // HFA requires 2-4 members (1 member is just a scalar, >4 is not HFA)
  if (Count >= 2 && Count <= 4) {
    MemberClass = FirstClass;
    MemberCount = Count;
    return true;
  }

  return false;
}

bool AArch64ABIInfo::isHVA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
                            unsigned &MemberCount) const {
  MemberCount = 0;
  MemberClass = AArch64ABIPrimitiveClass::Integer;

  // HVA: Homogeneous Vector Aggregate — 2-4 identical vector members
  // BlockType doesn't have vector types yet, so this always returns false.
  // Will be implemented when vector types are added.
  (void)Ty;
  return false;
}

bool AArch64ABIInfo::isHFPA(QualType Ty, AArch64ABIPrimitiveClass &MemberClass,
                             unsigned &MemberCount) const {
  return isHFA(Ty, MemberClass, MemberCount) ||
         isHVA(Ty, MemberClass, MemberCount);
}

AArch64ABIPrimitiveClass AArch64ABIInfo::classifyRecord(QualType Ty) const {
  if (Ty.isNull()) return AArch64ABIPrimitiveClass::Memory;
  const Type *T = Ty.getTypePtr();
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (!RT) return AArch64ABIPrimitiveClass::Memory;

  RecordDecl *RD = RT->getDecl();
  if (!RD) return AArch64ABIPrimitiveClass::Memory;

  // Empty struct → MEMORY
  if (RD->fields().empty()) return AArch64ABIPrimitiveClass::Memory;

  // Check for HFA/HVA first
  AArch64ABIPrimitiveClass HFAClass;
  unsigned HFACount;
  if (isHFPA(Ty, HFAClass, HFACount))
    return HFAClass;

  // Classify by examining all member types
  AArch64ABIPrimitiveClass Result = AArch64ABIPrimitiveClass::Integer;
  for (FieldDecl *Field : RD->fields()) {
    QualType FT = Field->getType();
    AArch64ABIPrimitiveClass FC;

    if (FT->isRecordType()) {
      FC = classifyRecord(FT);
    } else {
      FC = classifyPrimitive(FT);
    }

    // If any member is MEMORY → whole struct is MEMORY
    if (FC == AArch64ABIPrimitiveClass::Memory)
      return AArch64ABIPrimitiveClass::Memory;

    // Mixed FP and Integer → MEMORY (AAPCS64 rule)
    if (Result != FC) {
      if (Result == AArch64ABIPrimitiveClass::Integer && FC != AArch64ABIPrimitiveClass::Integer) {
        // First non-integer member
        Result = FC;
      } else if (Result != AArch64ABIPrimitiveClass::Integer &&
                 FC == AArch64ABIPrimitiveClass::Integer) {
        // Mixed FP + Integer → MEMORY
        return AArch64ABIPrimitiveClass::Memory;
      } else {
        // Mixed FP types → MEMORY
        return AArch64ABIPrimitiveClass::Memory;
      }
    }
  }

  return Result;
}

ABIArgInfo AArch64ABIInfo::classifyReturnType(QualType RetTy) const {
  if (RetTy.isNull()) return ABIArgInfo::getDirect();

  const Type *T = RetTy.getTypePtr();

  // Void → Direct
  if (T->isVoidType()) return ABIArgInfo::getDirect();

  // Non-record types: Direct
  if (!T->isRecordType()) return ABIArgInfo::getDirect();

  // Record types
  uint64_t Size = computeRecordSize(RetTy);

  // > 16 bytes → sret
  if (Size > 16)
    return ABIArgInfo::getSRet(nullptr);

  // Empty struct → Direct (AAPCS64: zero-sized struct is ignored)
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (RT && RT->getDecl() && RT->getDecl()->fields().empty())
    return ABIArgInfo::getDirect();

  // Check HFA/HVA
  AArch64ABIPrimitiveClass HFAClass;
  unsigned HFACount;
  if (isHFPA(RetTy, HFAClass, HFACount)) {
    // HFA/HVA: passed in FP/SIMD registers → Direct
    return ABIArgInfo::getDirect();
  }

  // Classify the record
  AArch64ABIPrimitiveClass Cls = classifyRecord(RetTy);

  if (Cls == AArch64ABIPrimitiveClass::Memory)
    return ABIArgInfo::getSRet(nullptr);

  // INTEGER or FP class, ≤16 bytes → Direct or Expand
  if (Size <= 8)
    return ABIArgInfo::getDirect();

  // 9-16 bytes: may need Expand if struct has multiple members
  if (RT && RT->getDecl()) {
    unsigned NumFields = RT->getDecl()->fields().size();
    if (NumFields > 1)
      return ABIArgInfo::getExpand();
  }

  return ABIArgInfo::getDirect();
}

ABIArgInfo AArch64ABIInfo::classifyArgumentType(QualType ParamTy) const {
  if (ParamTy.isNull()) return ABIArgInfo::getDirect();

  const Type *T = ParamTy.getTypePtr();

  // Non-record types: Direct
  if (!T->isRecordType()) return ABIArgInfo::getDirect();

  // Record types
  uint64_t Size = computeRecordSize(ParamTy);

  // > 16 bytes → memory (passed indirectly)
  if (Size > 16)
    return ABIArgInfo::getDirect();

  // Check HFA/HVA
  AArch64ABIPrimitiveClass HFAClass;
  unsigned HFACount;
  if (isHFPA(ParamTy, HFAClass, HFACount)) {
    // HFA/HVA: passed in FP/SIMD registers → Direct
    return ABIArgInfo::getDirect();
  }

  // Classify the record
  AArch64ABIPrimitiveClass Cls = classifyRecord(ParamTy);

  if (Cls == AArch64ABIPrimitiveClass::Memory)
    return ABIArgInfo::getDirect();

  // INTEGER or FP class, ≤16 bytes → Direct or Expand
  if (Size <= 8)
    return ABIArgInfo::getDirect();

  // 9-16 bytes: may need Expand
  auto *RT = llvm::dyn_cast<RecordType>(T);
  if (RT && RT->getDecl()) {
    unsigned NumFields = RT->getDecl()->fields().size();
    if (NumFields > 1)
      return ABIArgInfo::getExpand();
  }

  return ABIArgInfo::getDirect();
}
