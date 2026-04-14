#pragma once

#include "llvm/ADT/StringRef.h"

namespace blocktype {

class FileEntry {
  llvm::StringRef Name;
  llvm::StringRef Path;
  size_t Size = 0;
  unsigned UID = 0;
public:
  FileEntry(llvm::StringRef N, llvm::StringRef P, size_t S, unsigned U)
    : Name(N), Path(P), Size(S), UID(U) {}
  
  llvm::StringRef getName() const { return Name; }
  llvm::StringRef getPath() const { return Path; }
  size_t getSize() const { return Size; }
  unsigned getUID() const { return UID; }
};

} // namespace blocktype