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
    // Return a copy of the cached buffer (already converted to UTF-8)
    return llvm::MemoryBuffer::getMemBufferCopy(it->second->getBuffer());
  }

  // Read file into buffer
  auto buf = llvm::MemoryBuffer::getFile(Path);
  if (!buf) {
    return nullptr;
  }

  auto result = std::move(*buf);

  // Detect encoding and convert to UTF-8 if needed
  Encoding Enc = detectEncoding(result->getBufferStart(), result->getBufferSize());
  if (Enc != Encoding::UTF8 && Enc != Encoding::Unknown) {
    auto Converted = convertToUTF8(result.get());
    if (Converted) {
      // Cache the converted buffer
      BufferCache[Path.str()] = llvm::MemoryBuffer::getMemBufferCopy(Converted->getBuffer());
      return Converted;
    }
  }

  // Cache the buffer (UTF-8 or unknown encoding)
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

std::string FileManager::convertUTF16ToUTF8(const char *Data, size_t Length, bool BigEndian) {
  std::string Result;

  // Skip BOM if present
  size_t Start = 0;
  if (Length >= 2) {
    unsigned char BOM0 = static_cast<unsigned char>(Data[0]);
    unsigned char BOM1 = static_cast<unsigned char>(Data[1]);
    if ((BigEndian && BOM0 == 0xFE && BOM1 == 0xFF) ||
        (!BigEndian && BOM0 == 0xFF && BOM1 == 0xFE)) {
      Start = 2;
    }
  }

  // Process UTF-16 code units
  for (size_t i = Start; i + 1 < Length; i += 2) {
    uint16_t CodeUnit;
    if (BigEndian) {
      CodeUnit = (static_cast<uint16_t>(static_cast<unsigned char>(Data[i])) << 8) |
                  static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 1]));
    } else {
      CodeUnit = static_cast<uint16_t>(static_cast<unsigned char>(Data[i])) |
                 (static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 1])) << 8);
    }

    // Handle surrogate pairs
    if (CodeUnit >= 0xD800 && CodeUnit <= 0xDBFF) {
      // High surrogate
      if (i + 3 >= Length) break;
      uint16_t LowSurrogate;
      if (BigEndian) {
        LowSurrogate = (static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 2])) << 8) |
                        static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 3]));
      } else {
        LowSurrogate = static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 2])) |
                        (static_cast<uint16_t>(static_cast<unsigned char>(Data[i + 3])) << 8);
      }
      if (LowSurrogate >= 0xDC00 && LowSurrogate <= 0xDFFF) {
        // Valid surrogate pair
        uint32_t CodePoint = 0x10000 + ((CodeUnit - 0xD800) << 10) + (LowSurrogate - 0xDC00);
        // Encode as UTF-8
        Result += static_cast<char>(0xF0 | ((CodePoint >> 18) & 0x07));
        Result += static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F));
        Result += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F));
        Result += static_cast<char>(0x80 | (CodePoint & 0x3F));
        i += 2;  // Skip low surrogate
        continue;
      }
    }

    // Regular UTF-16 code unit
    if (CodeUnit < 0x80) {
      Result += static_cast<char>(CodeUnit);
    } else if (CodeUnit < 0x800) {
      Result += static_cast<char>(0xC0 | ((CodeUnit >> 6) & 0x1F));
      Result += static_cast<char>(0x80 | (CodeUnit & 0x3F));
    } else {
      Result += static_cast<char>(0xE0 | ((CodeUnit >> 12) & 0x0F));
      Result += static_cast<char>(0x80 | ((CodeUnit >> 6) & 0x3F));
      Result += static_cast<char>(0x80 | (CodeUnit & 0x3F));
    }
  }

  return Result;
}

std::string FileManager::convertUTF32ToUTF8(const char *Data, size_t Length, bool BigEndian) {
  std::string Result;

  // Skip BOM if present
  size_t Start = 0;
  if (Length >= 4) {
    uint32_t BOM;
    if (BigEndian) {
      BOM = (static_cast<uint32_t>(static_cast<unsigned char>(Data[0])) << 24) |
            (static_cast<uint32_t>(static_cast<unsigned char>(Data[1])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(Data[2])) << 8) |
            static_cast<uint32_t>(static_cast<unsigned char>(Data[3]));
    } else {
      BOM = static_cast<uint32_t>(static_cast<unsigned char>(Data[0])) |
            (static_cast<uint32_t>(static_cast<unsigned char>(Data[1])) << 8) |
            (static_cast<uint32_t>(static_cast<unsigned char>(Data[2])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(Data[3])) << 24);
    }
    if (BOM == 0x0000FEFF || BOM == 0xFFFE0000) {
      Start = 4;
    }
  }

  // Process UTF-32 code points
  for (size_t i = Start; i + 3 < Length; i += 4) {
    uint32_t CodePoint;
    if (BigEndian) {
      CodePoint = (static_cast<uint32_t>(static_cast<unsigned char>(Data[i])) << 24) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 1])) << 16) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 2])) << 8) |
                  static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 3]));
    } else {
      CodePoint = static_cast<uint32_t>(static_cast<unsigned char>(Data[i])) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 1])) << 8) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 2])) << 16) |
                  (static_cast<uint32_t>(static_cast<unsigned char>(Data[i + 3])) << 24);
    }

    // Encode as UTF-8
    if (CodePoint < 0x80) {
      Result += static_cast<char>(CodePoint);
    } else if (CodePoint < 0x800) {
      Result += static_cast<char>(0xC0 | ((CodePoint >> 6) & 0x1F));
      Result += static_cast<char>(0x80 | (CodePoint & 0x3F));
    } else if (CodePoint < 0x10000) {
      Result += static_cast<char>(0xE0 | ((CodePoint >> 12) & 0x0F));
      Result += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F));
      Result += static_cast<char>(0x80 | (CodePoint & 0x3F));
    } else if (CodePoint < 0x110000) {
      Result += static_cast<char>(0xF0 | ((CodePoint >> 18) & 0x07));
      Result += static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F));
      Result += static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F));
      Result += static_cast<char>(0x80 | (CodePoint & 0x3F));
    }
  }

  return Result;
}

std::unique_ptr<llvm::MemoryBuffer> FileManager::convertToUTF8(llvm::MemoryBuffer *Buffer) {
  if (!Buffer) return nullptr;

  const char *Data = Buffer->getBufferStart();
  size_t Length = Buffer->getBufferSize();

  Encoding Enc = detectEncoding(Data, Length);

  // Already UTF-8 or unknown, return as-is (remove BOM if present)
  if (Enc == Encoding::UTF8) {
    size_t Start = 0;
    if (Length >= 3 && static_cast<unsigned char>(Data[0]) == 0xEF &&
        static_cast<unsigned char>(Data[1]) == 0xBB &&
        static_cast<unsigned char>(Data[2]) == 0xBF) {
      Start = 3;
    }
    return llvm::MemoryBuffer::getMemBufferCopy(
        llvm::StringRef(Data + Start, Length - Start), Buffer->getBufferIdentifier());
  }

  std::string UTF8Content;

  switch (Enc) {
    case Encoding::UTF16_LE:
      UTF8Content = convertUTF16ToUTF8(Data, Length, false);
      break;
    case Encoding::UTF16_BE:
      UTF8Content = convertUTF16ToUTF8(Data, Length, true);
      break;
    case Encoding::UTF32_LE:
      UTF8Content = convertUTF32ToUTF8(Data, Length, false);
      break;
    case Encoding::UTF32_BE:
      UTF8Content = convertUTF32ToUTF8(Data, Length, true);
      break;
    default:
      // Unknown encoding, return as-is
      return llvm::MemoryBuffer::getMemBufferCopy(Buffer->getBuffer(), Buffer->getBufferIdentifier());
  }

  return llvm::MemoryBuffer::getMemBufferCopy(UTF8Content, Buffer->getBufferIdentifier());
}

} // namespace blocktype
