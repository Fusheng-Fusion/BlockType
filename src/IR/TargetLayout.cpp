#include "blocktype/IR/TargetLayout.h"
#include "blocktype/IR/IRType.h"

#include <algorithm>
#include <cassert>

namespace blocktype {
namespace ir {

TargetLayout::TargetLayout(std::string_view TargetTriple)
  : TripleStr(TargetTriple) {
  PointerSize = 8;
  PointerAlign = 8;
  IntSize = 4;
  LongSize = 8;
  LongLongSize = 8;
  FloatSize = 4;
  DoubleSize = 8;
  LongDoubleSize = 16;
  MaxVectorAlign = 16;
  IsLittleEndian = true;
  IsMacOS = false;
  IsLinux = false;

  if (TargetTriple.find("apple") != std::string::npos ||
      TargetTriple.find("macos") != std::string::npos ||
      TargetTriple.find("darwin") != std::string::npos) {
    IsMacOS = true;
  } else {
    IsLinux = true;
  }

  bool IsARM64 = TargetTriple.find("aarch64") != std::string::npos ||
                  TargetTriple.find("arm64") != std::string::npos;
  bool IsX86_64 = TargetTriple.find("x86_64") != std::string::npos ||
                  TargetTriple.find("x86-64") != std::string::npos ||
                  TargetTriple.find("amd64") != std::string::npos;

  if (IsARM64 || IsX86_64) {
    LongDoubleSize = 16;
    MaxVectorAlign = 16;
  }

  (void)IsARM64;
  (void)IsX86_64;

  if (TargetTriple.find("big-endian") != std::string::npos ||
      TargetTriple.find("eb") != std::string::npos) {
    IsLittleEndian = false;
  }

  (void)IsLinux;
}

uint64_t TargetLayout::getTypeSizeInBits(IRType* T) const {
  return T->getSizeInBits(*this);
}

uint64_t TargetLayout::getTypeAlignInBits(IRType* T) const {
  return T->getAlignInBits(*this);
}

std::unique_ptr<TargetLayout> TargetLayout::Create(std::string_view Triple) {
  return std::make_unique<TargetLayout>(Triple);
}

} // namespace ir
} // namespace blocktype
