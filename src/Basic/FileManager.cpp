#include "blocktype/Basic/FileManager.h"
#include "llvm/Support/MemoryBuffer.h"
#include <fstream>
#include <cstring>

namespace blocktype {

const FileEntry* FileManager::getFile(llvm::StringRef Path) {
  // Check cache first
  auto it = FileCache.find(Path.str());
  if (it != FileCache.end()) {
    return it->second.get();
  }
  
  // Try to open file
  std::ifstream file(Path.str(), std::ios::binary | std::ios::ate);
  if (!file) {
    return nullptr;
  }
  
  // Get file size
  size_t size = file.tellg();
  file.close();
  
  // Create file entry
  auto entry = std::make_unique<FileEntry>(
    Path.rsplit('/').second, Path, size, NextUID++);
  
  FileEntry* result = entry.get();
  FileCache[Path.str()] = std::move(entry);
  
  return result;
}

std::unique_ptr<llvm::MemoryBuffer> FileManager::getBuffer(llvm::StringRef Path) {
  // Check buffer cache
  auto it = BufferCache.find(Path.str());
  if (it != BufferCache.end()) {
    // Return a copy of the cached buffer
    return llvm::MemoryBuffer::getMemBufferCopy(it->second->getBuffer());
  }
  
  // Read file into buffer
  auto buf = llvm::MemoryBuffer::getFile(Path);
  if (!buf) {
    return nullptr;
  }
  
  // Cache the buffer
  auto result = std::move(*buf);
  BufferCache[Path.str()] = llvm::MemoryBuffer::getMemBufferCopy(result->getBuffer());
  
  return result;
}

llvm::MemoryBuffer* FileManager::getBufferRef(llvm::StringRef Path) {
  auto it = BufferCache.find(Path.str());
  if (it != BufferCache.end()) {
    return it->second.get();
  }
  
  // Read and cache
  auto buf = llvm::MemoryBuffer::getFile(Path);
  if (!buf) {
    return nullptr;
  }
  
  BufferCache[Path.str()] = std::move(*buf);
  return BufferCache[Path.str()].get();
}

bool FileManager::exists(llvm::StringRef Path) const {
  return std::ifstream(Path.str()).good();
}

FileManager::Encoding FileManager::detectEncoding(const char *Data, size_t Length) {
  if (Length < 2) {
    return Encoding::Unknown;
  }
  
  // Check for BOM
  unsigned char BOM0 = static_cast<unsigned char>(Data[0]);
  unsigned char BOM1 = static_cast<unsigned char>(Data[1]);
  
  if (Length >= 3 && BOM0 == 0xEF && BOM1 == 0xBB && 
      static_cast<unsigned char>(Data[2]) == 0xBF) {
    return Encoding::UTF8;
  }
  
  if (BOM0 == 0xFF && BOM1 == 0xFE) {
    if (Length >= 4 && static_cast<unsigned char>(Data[2]) == 0x00 && 
        static_cast<unsigned char>(Data[3]) == 0x00) {
      return Encoding::UTF32_LE;
    }
    return Encoding::UTF16_LE;
  }
  
  if (BOM0 == 0xFE && BOM1 == 0xFF) {
    return Encoding::UTF16_BE;
  }
  
  if (Length >= 4 && BOM0 == 0x00 && BOM1 == 0x00) {
    if (static_cast<unsigned char>(Data[2]) == 0xFE && 
        static_cast<unsigned char>(Data[3]) == 0xFF) {
      return Encoding::UTF32_BE;
    }
  }
  
  // No BOM found, assume UTF-8 or ASCII
  return Encoding::UTF8;
}

FileManager::Encoding FileManager::detectFileEncoding(llvm::StringRef Path) {
  std::ifstream file(Path.str(), std::ios::binary);
  if (!file) {
    return Encoding::Unknown;
  }
  
  char buffer[4];
  file.read(buffer, 4);
  size_t bytesRead = file.gcount();
  
  return detectEncoding(buffer, bytesRead);
}

} // namespace blocktype
