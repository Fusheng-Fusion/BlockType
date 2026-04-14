#pragma once

#include "blocktype/Basic/FileEntry.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>
#include <map>
#include <string>

namespace blocktype {

/// FileManager - Manages file lookup and caching
class FileManager {
  std::map<std::string, std::unique_ptr<FileEntry>> FileCache;
  std::map<std::string, std::unique_ptr<llvm::MemoryBuffer>> BufferCache;
  unsigned NextUID = 0;

public:
  /// Get a file entry for the given path
  const FileEntry* getFile(llvm::StringRef Path);

  /// Get a memory buffer for the given path (cached)
  std::unique_ptr<llvm::MemoryBuffer> getBuffer(llvm::StringRef Path);

  /// Get a cached buffer without transferring ownership
  llvm::MemoryBuffer* getBufferRef(llvm::StringRef Path);

  /// Check if a file exists
  bool exists(llvm::StringRef Path) const;

  /// Clear all caches
  void clearCache() {
    FileCache.clear();
    BufferCache.clear();
  }

  /// Get the number of cached files
  size_t getNumCachedFiles() const { return FileCache.size(); }

  /// Detect file encoding (UTF-8, UTF-16, UTF-32)
  enum class Encoding {
    Unknown,
    UTF8,
    UTF16_LE,
    UTF16_BE,
    UTF32_LE,
    UTF32_BE
  };
  
  /// Detect encoding of a buffer
  static Encoding detectEncoding(const char *Data, size_t Length);
  
  /// Detect encoding of a file
  Encoding detectFileEncoding(llvm::StringRef Path);
};

} // namespace blocktype
