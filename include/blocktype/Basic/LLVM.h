#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Error.h"

namespace blocktype {
using llvm::StringRef;
using llvm::SmallVector;
using llvm::ArrayRef;
using llvm::StringMap;
using llvm::raw_ostream;
using llvm::MemoryBuffer;
using llvm::Expected;
} // namespace blocktype