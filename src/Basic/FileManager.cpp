#include "blocktype/Basic/FileManager.h"
#include "llvm/Support/MemoryBuffer.h"
#include <fstream>

namespace blocktype {

const FileEntry* FileManager::getFile(llvm::StringRef Path) {
  auto it = FileCache.find(Path.str());
  if (it != FileCache.end()) return it->second.get();
  
  std::ifstream file(Path.str(), std::ios::binary | std::ios::ate);
  if (!file) return nullptr;
  
  size_t size = file.tellg();
  auto entry = std::make_unique<FileEntry>(
    Path.rsplit('/').second, Path, size, NextUID++);
  FileCache[Path.str()] = std::move(entry);
  return FileCache[Path.str()].get();
}

std::unique_ptr<llvm::MemoryBuffer> FileManager::getBuffer(llvm::StringRef Path) {
  auto buf = llvm::MemoryBuffer::getFile(Path);
  if (buf) return std::move(*buf);
  return nullptr;
}

bool FileManager::exists(llvm::StringRef Path) const {
  return std::ifstream(Path.str()).good();
}

} // namespace blocktype