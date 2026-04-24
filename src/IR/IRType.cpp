#include "blocktype/IR/IRType.h"

#include <algorithm>

namespace blocktype {
namespace ir {

static uint64_t alignTo(uint64_t Value, uint64_t Align) {
  return (Value + Align - 1) & ~(Align - 1);
}

bool IRIntegerType::equals(const IRType* Other) const {
  if (!Other->isInteger()) return false;
  return BitWidth == static_cast<const IRIntegerType*>(Other)->BitWidth;
}

std::string IRIntegerType::toString() const {
  return "i" + std::to_string(BitWidth);
}

uint64_t IRIntegerType::getAlignInBits(const TargetLayout& Layout) const {
  uint64_t Size = getSizeInBits(Layout);
  uint64_t PtrSize = Layout.getPointerSizeInBits();
  return std::min(Size, PtrSize);
}

bool IRFloatType::equals(const IRType* Other) const {
  if (!Other->isFloat()) return false;
  return BitWidth == static_cast<const IRFloatType*>(Other)->BitWidth;
}

std::string IRFloatType::toString() const {
  switch (BitWidth) {
  case 16: return "f16";
  case 32: return "f32";
  case 64: return "f64";
  case 80: return "f80";
  case 128: return "f128";
  default: return "f" + std::to_string(BitWidth);
  }
}

uint64_t IRFloatType::getAlignInBits(const TargetLayout& Layout) const {
  return getSizeInBits(Layout);
}

bool IRPointerType::equals(const IRType* Other) const {
  if (!Other->isPointer()) return false;
  auto* O = static_cast<const IRPointerType*>(Other);
  return PointeeType->equals(O->PointeeType) && AddressSpace == O->AddressSpace;
}

std::string IRPointerType::toString() const {
  std::string S = PointeeType->toString() + "*";
  if (AddressSpace != 0)
    S += " addrspace(" + std::to_string(AddressSpace) + ")";
  return S;
}

uint64_t IRPointerType::getSizeInBits(const TargetLayout& Layout) const {
  return Layout.getPointerSizeInBits();
}

uint64_t IRPointerType::getAlignInBits(const TargetLayout& Layout) const {
  return Layout.getPointerSizeInBits();
}

bool IRArrayType::equals(const IRType* Other) const {
  if (!Other->isArray()) return false;
  auto* O = static_cast<const IRArrayType*>(Other);
  return NumElements == O->NumElements && ElementType->equals(O->ElementType);
}

std::string IRArrayType::toString() const {
  return "[" + std::to_string(NumElements) + " x " + ElementType->toString() + "]";
}

uint64_t IRArrayType::getSizeInBits(const TargetLayout& Layout) const {
  return NumElements * ElementType->getSizeInBits(Layout);
}

uint64_t IRArrayType::getAlignInBits(const TargetLayout& Layout) const {
  return ElementType->getAlignInBits(Layout);
}

uint64_t IRStructType::getFieldOffset(unsigned i, const TargetLayout& Layout) const {
  assert(i < FieldTypes.size() && "Field index out of range");
  if (!IsLayoutComputed) {
    uint64_t Offset = 0;
    for (size_t Idx = 0; Idx < FieldTypes.size(); ++Idx) {
      uint64_t FieldSize = FieldTypes[Idx]->getSizeInBits(Layout);
      if (IsPacked) {
        FieldOffsets.push_back(Offset);
        Offset += FieldSize;
      } else {
        uint64_t FieldAlign = FieldTypes[Idx]->getAlignInBits(Layout);
        Offset = alignTo(Offset, FieldAlign);
        FieldOffsets.push_back(Offset);
        Offset += FieldSize;
      }
    }
    IsLayoutComputed = true;
  }
  return FieldOffsets[i];
}

bool IRStructType::equals(const IRType* Other) const {
  if (!Other->isStruct()) return false;
  auto* O = static_cast<const IRStructType*>(Other);
  if (Name != O->Name || IsPacked != O->IsPacked) return false;
  if (FieldTypes.size() != O->FieldTypes.size()) return false;
  for (size_t i = 0; i < FieldTypes.size(); ++i) {
    if (!FieldTypes[i]->equals(O->FieldTypes[i])) return false;
  }
  return true;
}

std::string IRStructType::toString() const {
  if (!Name.empty()) return "%" + Name;
  std::string S = "{ ";
  for (size_t i = 0; i < FieldTypes.size(); ++i) {
    if (i > 0) S += ", ";
    S += FieldTypes[i]->toString();
  }
  S += " }";
  return S;
}

uint64_t IRStructType::getSizeInBits(const TargetLayout& Layout) const {
  if (FieldTypes.empty()) return 0;
  uint64_t Offset = 0;
  for (size_t i = 0; i < FieldTypes.size(); ++i) {
    uint64_t FieldSize = FieldTypes[i]->getSizeInBits(Layout);
    if (IsPacked) {
      Offset += FieldSize;
    } else {
      uint64_t FieldAlign = FieldTypes[i]->getAlignInBits(Layout);
      Offset = alignTo(Offset, FieldAlign);
      Offset += FieldSize;
    }
  }
  if (!IsPacked) {
    uint64_t StructAlign = getAlignInBits(Layout);
    Offset = alignTo(Offset, StructAlign);
  }
  return Offset;
}

uint64_t IRStructType::getAlignInBits(const TargetLayout& Layout) const {
  if (IsPacked) return 8;
  uint64_t MaxAlign = 8;
  for (auto* FT : FieldTypes) {
    uint64_t FA = FT->getAlignInBits(Layout);
    if (FA > MaxAlign) MaxAlign = FA;
  }
  return MaxAlign;
}

bool IRFunctionType::equals(const IRType* Other) const {
  if (!Other->isFunction()) return false;
  auto* O = static_cast<const IRFunctionType*>(Other);
  if (!ReturnType->equals(O->ReturnType)) return false;
  if (IsVarArg != O->IsVarArg) return false;
  if (ParamTypes.size() != O->ParamTypes.size()) return false;
  for (size_t i = 0; i < ParamTypes.size(); ++i) {
    if (!ParamTypes[i]->equals(O->ParamTypes[i])) return false;
  }
  return true;
}

std::string IRFunctionType::toString() const {
  std::string S = ReturnType->toString() + " (";
  for (size_t i = 0; i < ParamTypes.size(); ++i) {
    if (i > 0) S += ", ";
    S += ParamTypes[i]->toString();
  }
  if (IsVarArg) {
    if (!ParamTypes.empty()) S += ", ";
    S += "...";
  }
  S += ")";
  return S;
}

bool IRVectorType::equals(const IRType* Other) const {
  if (!Other->isVector()) return false;
  auto* O = static_cast<const IRVectorType*>(Other);
  return NumElements == O->NumElements && ElementType->equals(O->ElementType);
}

std::string IRVectorType::toString() const {
  return "<" + std::to_string(NumElements) + " x " + ElementType->toString() + ">";
}

uint64_t IRVectorType::getSizeInBits(const TargetLayout& Layout) const {
  return NumElements * ElementType->getSizeInBits(Layout);
}

uint64_t IRVectorType::getAlignInBits(const TargetLayout& Layout) const {
  return getSizeInBits(Layout);
}

bool IROpaqueType::equals(const IRType* Other) const {
  if (!Other->isOpaque()) return false;
  return Name == static_cast<const IROpaqueType*>(Other)->Name;
}

std::string IROpaqueType::toString() const {
  return "opaque %" + Name;
}

uint64_t IROpaqueType::getSizeInBits(const TargetLayout&) const {
  assert(false && "IROpaqueType::getSizeInBits cannot be called");
  return 0;
}

uint64_t IROpaqueType::getAlignInBits(const TargetLayout&) const {
  assert(false && "IROpaqueType::getAlignInBits cannot be called");
  return 0;
}

} // namespace ir
} // namespace blocktype
