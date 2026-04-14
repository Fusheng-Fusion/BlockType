//===--- SourceManager.cpp - Source Manager --------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the SourceManager class.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>

namespace blocktype {

//===----------------------------------------------------------------------===//
// FileInfo Implementation
//===----------------------------------------------------------------------===//

FileInfo::FileInfo(StringRef Name, StringRef Data, unsigned ID)
    : Filename(Name.str()), Content(Data), FileID(ID) {
  // Compute line offsets
  LineOffsets.push_back(0); // First line starts at offset 0
  
  for (size_t i = 0; i < Content.size(); ++i) {
    if (Content[i] == '\n') {
      LineOffsets.push_back(static_cast<unsigned>(i + 1));
    }
  }
}

std::pair<unsigned, unsigned> FileInfo::getLineAndColumn(unsigned Offset) const {
  // Binary search for the line containing this offset
  auto It = std::upper_bound(LineOffsets.begin(), LineOffsets.end(), Offset);
  
  unsigned Line;
  if (It == LineOffsets.begin()) {
    Line = 1;
  } else {
    --It;
    Line = static_cast<unsigned>(std::distance(LineOffsets.begin(), It) + 1);
  }

  unsigned Column = Offset - getLineOffset(Line) + 1;
  return {Line, Column};
}

unsigned FileInfo::getLineOffset(unsigned Line) const {
  if (Line == 0 || Line > LineOffsets.size())
    return 0;
  return LineOffsets[Line - 1];
}

//===----------------------------------------------------------------------===//
// SourceManager Implementation
//===----------------------------------------------------------------------===//

SourceLocation SourceManager::createMainFileID(StringRef Filename, StringRef Content) {
  if (!Files.empty()) {
    // Main file already exists, replace it
    Files[0] = std::make_unique<FileInfo>(Filename, Content, 0);
    return SourceLocation(0);
  }
  
  return createFileID(Filename, Content);
}

SourceLocation SourceManager::createFileID(StringRef Filename, StringRef Content) {
  unsigned FileID = static_cast<unsigned>(Files.size());
  Files.push_back(std::make_unique<FileInfo>(Filename, Content, FileID));
  
  // Encode file ID in source location (simple encoding: just use file ID)
  return SourceLocation(FileID);
}

const FileInfo *SourceManager::getFileInfo(SourceLocation Loc) const {
  if (!Loc.isValid())
    return nullptr;
  
  unsigned FileID = Loc.getID();
  return getFileInfo(FileID);
}

const FileInfo *SourceManager::getFileInfo(unsigned FileID) const {
  if (FileID >= Files.size())
    return nullptr;
  return Files[FileID].get();
}

std::pair<unsigned, unsigned> SourceManager::getLineAndColumn(SourceLocation Loc) const {
  const FileInfo *FI = getFileInfo(Loc);
  if (!FI)
    return {0, 0};
  
  // For now, we use a simple encoding where Loc.getID() is the file ID
  // In a real implementation, we'd have offset information encoded
  // This is a placeholder that returns line 1, column 1
  return {1, 1};
}

StringRef SourceManager::getCharacterData(SourceLocation Loc) const {
  const FileInfo *FI = getFileInfo(Loc);
  if (!FI)
    return StringRef();
  
  return FI->getContent();
}

StringRef SourceManager::getCharacterData(SourceLocation Start, SourceLocation End) const {
  // Placeholder implementation
  return getCharacterData(Start);
}

void SourceManager::printLocation(raw_ostream &OS, SourceLocation Loc) const {
  std::string Str = getLocationString(Loc);
  OS << Str;
}

std::string SourceManager::getLocationString(SourceLocation Loc) const {
  const FileInfo *FI = getFileInfo(Loc);
  if (!FI)
    return "<invalid loc>";
  
  auto [Line, Column] = getLineAndColumn(Loc);
  return FI->getFilename().str() + ":" + std::to_string(Line) + ":" + 
         std::to_string(Column);
}

} // namespace blocktype
