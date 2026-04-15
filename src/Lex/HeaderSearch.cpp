//===--- HeaderSearch.cpp - Header Search Implementation -------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Lex/HeaderSearch.h"
#include "blocktype/Basic/FileEntry.h"
#include "blocktype/Basic/FileManager.h"
#include <sys/stat.h>

namespace blocktype {

HeaderSearch::HeaderSearch(FileManager &FM) : FileMgr(FM) {}

void HeaderSearch::addSearchPath(StringRef Path, bool IsSystem, bool IsFramework) {
  SearchPaths.push_back({Path.str(), IsSystem, IsFramework});
}

const FileEntry *HeaderSearch::lookupHeader(StringRef Filename, bool IsAngled,
                                            StringRef IncludeDir) {
  // D13: Check lookup cache first
  auto CacheKey = std::make_tuple(Filename.str(), IsAngled, IncludeDir.str());
  auto It = LookupCache.find(CacheKey);
  if (It != LookupCache.end()) {
    ++LookupCacheHits;
    return It->second;
  }
  ++LookupCacheMisses;

  const FileEntry *Result = nullptr;

  // Absolute path - search directly
  if (isAbsolutePath(Filename)) {
    Result = FileMgr.getFile(Filename);
    LookupCache[CacheKey] = Result;
    return Result;
  }

  // For "..." style includes, first search in including directory
  if (!IsAngled && !IncludeDir.empty()) {
    std::string FullPath = joinPath(IncludeDir, Filename);
    if (cachedFileExists(FullPath)) {
      Result = FileMgr.getFile(FullPath);
      if (Result) {
        LookupCache[CacheKey] = Result;
        return Result;
      }
    }
  }

  // Search in search paths
  for (const auto &SP : SearchPaths) {
    // For <...> style, skip non-system paths if we want system headers
    // For "..." style, search all paths
    if (IsAngled && !SP.IsSystem) {
      continue;
    }

    const FileEntry *FE = nullptr;
    if (SP.IsFramework) {
      FE = searchFramework(SP.Path, Filename);
    } else {
      FE = searchInPath(SP.Path, Filename);
    }

    if (FE) {
      Result = FE;
      break;
    }
  }

  // D13: Cache the result (including nullptr for not found)
  LookupCache[CacheKey] = Result;
  return Result;
}

bool HeaderSearch::headerExists(StringRef Filename, bool IsAngled) {
  return lookupHeader(Filename, IsAngled) != nullptr;
}

const FileEntry *HeaderSearch::lookupHeaderNext(StringRef Filename, StringRef CurrentFile) {
  // Find the search path where the current file was found
  size_t CurrentPathIndex = 0;
  bool FoundCurrent = false;

  for (size_t i = 0; i < SearchPaths.size(); ++i) {
    std::string FullPath = joinPath(SearchPaths[i].Path, CurrentFile);
    if (FileMgr.getFile(FullPath)) {
      CurrentPathIndex = i;
      FoundCurrent = true;
      break;
    }
  }

  // If current file not found in search paths, start from beginning
  if (!FoundCurrent) {
    CurrentPathIndex = 0;
  } else {
    // Start from next path after current
    CurrentPathIndex++;
  }

  // Search starting from next path
  for (size_t i = CurrentPathIndex; i < SearchPaths.size(); ++i) {
    const auto &SP = SearchPaths[i];
    const FileEntry *FE = nullptr;
    if (SP.IsFramework) {
      FE = searchFramework(SP.Path, Filename);
    } else {
      FE = searchInPath(SP.Path, Filename);
    }
    if (FE) {
      return FE;
    }
  }

  return nullptr;
}

void HeaderSearch::markIncluded(StringRef Filename) {
  IncludedFiles.insert(Filename.str());
}

bool HeaderSearch::wasIncluded(StringRef Filename) const {
  return IncludedFiles.find(Filename.str()) != IncludedFiles.end();
}

void HeaderSearch::markHasIncludeGuard(StringRef Filename) {
  IncludeGuardFiles.insert(Filename.str());
}

bool HeaderSearch::hasIncludeGuard(StringRef Filename) const {
  return IncludeGuardFiles.find(Filename.str()) != IncludeGuardFiles.end();
}

std::string HeaderSearch::getCanonicalPath(StringRef Filename) {
  // Implement proper path canonicalization
  // Handle: . (current directory), .. (parent directory), multiple slashes

  if (Filename.empty()) {
    return "";
  }

  std::string Result;
  std::vector<std::string> Components;

  // Split path into components
  bool IsAbs = isAbsolutePath(Filename);
  size_t Start = IsAbs ? 1 : 0;

  for (size_t Idx = Start; Idx < Filename.size(); ) {
    size_t Next = Filename.find('/', Idx);
    if (Next == StringRef::npos) {
      Next = Filename.size();
    }

    StringRef Component = Filename.substr(Idx, Next - Idx);
    Idx = Next + 1;

    if (Component.empty() || Component == ".") {
      // Skip empty components and current directory
      continue;
    }

    if (Component == "..") {
      // Parent directory - pop last component if possible
      if (!Components.empty() && Components.back() != "..") {
        Components.pop_back();
      } else if (!IsAbs) {
        // Can't go above root, keep ".." for relative paths
        Components.emplace_back("..");
      }
    } else {
      Components.emplace_back(Component.str());
    }
  }

  // Reconstruct path
  if (IsAbs) {
    Result = "/";
  }

  for (size_t Idx = 0; Idx < Components.size(); ++Idx) {
    if (Idx > 0) {
      Result += "/";
    }
    Result += Components[Idx];
  }

  return Result.empty() ? (IsAbs ? "/" : ".") : Result;
}

bool HeaderSearch::isAbsolutePath(StringRef Path) {
  if (Path.empty()) {
    return false;
  }
  return Path[0] == '/';
}

std::string HeaderSearch::joinPath(StringRef Dir, StringRef Filename) {
  if (Dir.empty()) {
    return Filename.str();
  }
  if (Dir.back() == '/') {
    return Dir.str() + Filename.str();
  }
  return Dir.str() + "/" + Filename.str();
}

const FileEntry *HeaderSearch::searchInPath(StringRef Path, StringRef Filename) {
  std::string FullPath = joinPath(Path, Filename);

  // D13: Use cached existence check
  if (!cachedFileExists(FullPath)) {
    return nullptr;
  }

  return FileMgr.getFile(FullPath);
}

const FileEntry *HeaderSearch::searchFramework(StringRef Path, StringRef Filename) {
  // Framework search follows macOS/iOS conventions:
  // 1. FrameworkName.framework/Headers/HeaderName
  // 2. FrameworkName.framework/Headers/HeaderName.h (if HeaderName doesn't have extension)
  // 3. FrameworkName.framework/PrivateHeaders/HeaderName (for private headers)
  // 4. FrameworkName.framework/Versions/A/Headers/HeaderName (versioned frameworks)
  // 5. FrameworkName.framework/Versions/Current/Headers/HeaderName

  std::string FrameworkName = Filename.str();

  // Extract header name (remove extension if present)
  std::string HeaderName = FrameworkName;
  size_t DotPos = HeaderName.rfind('.');
  bool HasExtension = (DotPos != std::string::npos && DotPos > 0);
  if (HasExtension) {
    FrameworkName = HeaderName.substr(0, DotPos);
  }

  // List of possible search locations within a framework
  std::vector<std::string> SearchLocations = {
    // Standard location
    "Headers",
    // Private headers
    "PrivateHeaders",
    // Versioned frameworks (A is the most common version)
    "Versions/A/Headers",
    "Versions/A/PrivateHeaders",
    // Current version (symlink to the current version)
    "Versions/Current/Headers",
    "Versions/Current/PrivateHeaders"
  };

  // Try each search location
  for (const auto& Location : SearchLocations) {
    std::string FrameworkPath = Path.str() + "/" + FrameworkName + ".framework/" +
                                Location + "/" + HeaderName;

    // D13: Use cached existence check
    if (cachedFileExists(FrameworkPath)) {
      return FileMgr.getFile(FrameworkPath);
    }

    // If header doesn't have extension, try adding .h
    if (!HasExtension) {
      std::string FrameworkPathWithExt = FrameworkPath + ".h";
      if (cachedFileExists(FrameworkPathWithExt)) {
        return FileMgr.getFile(FrameworkPathWithExt);
      }
    }
  }

  return nullptr;
}

bool HeaderSearch::cachedFileExists(StringRef Path) {
  // D13: Check stat cache first
  auto It = StatCache.find(Path.str());
  if (It != StatCache.end()) {
    ++StatCacheHits;
    return It->second;
  }
  ++StatCacheMisses;

  // Check file existence and cache result
  bool Exists = FileMgr.exists(Path);
  StatCache[Path.str()] = Exists;
  return Exists;
}

} // namespace blocktype