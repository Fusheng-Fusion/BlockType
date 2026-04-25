#ifndef BLOCKTYPE_IR_IRCACHE_H
#define BLOCKTYPE_IR_IRCACHE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRFormatVersion.h"

namespace blocktype {
namespace ir {
class IRModule;
class IRTypeContext;
} // namespace ir

namespace cache {

// ============================================================
// CacheOptions — 缓存键计算所需的编译选项子集
// ============================================================

struct CacheOptions {
  std::string TargetTriple;
  std::string DataLayout;
  uint64_t FeatureFlags = 0;
  ir::IRFormatVersion Version = ir::IRFormatVersion::Current();
  CacheOptions() = default;
};

// ============================================================
// CacheKey — 缓存键
// ============================================================

struct CacheKey {
  uint64_t SourceHash = 0;
  uint64_t OptionsHash = 0;
  uint64_t VersionHash = 0;
  uint64_t TargetTripleHash = 0;
  uint64_t DependencyHash = 0;
  uint64_t CombinedHash = 0;

  static CacheKey compute(ir::StringRef Source, const CacheOptions& Opts);
  std::string toHex() const;
  std::string toPathComponents() const;

  bool operator==(const CacheKey& O) const { return CombinedHash == O.CombinedHash; }
  bool operator!=(const CacheKey& O) const { return !(*this == O); }
};

// ============================================================
// CacheEntry — 缓存条目
// ============================================================

struct CacheEntry {
  CacheKey Key;
  std::vector<uint8_t> IRData;
  std::vector<uint8_t> ObjectData;
  ir::IRFormatVersion IRVersion = ir::IRFormatVersion::Current();
  uint64_t Timestamp = 0;
  size_t IRSize = 0;
  size_t ObjectSize = 0;

  static std::optional<CacheEntry> fromJSON(const std::string& JSON);
  std::string toJSON() const;
};

// ============================================================
// CacheStorage — 缓存存储抽象基类
// ============================================================

class CacheStorage {
public:
  virtual ~CacheStorage() = default;
  virtual std::optional<CacheEntry> lookup(const CacheKey& Key) = 0;
  virtual bool store(const CacheKey& Key, const CacheEntry& Entry) = 0;

  struct Stats { size_t Hits = 0; size_t Misses = 0; size_t Evictions = 0; size_t TotalSize = 0; };
  virtual Stats getStats() const = 0;
};

// ============================================================
// LocalDiskCache — 本地磁盘缓存
// ============================================================

class LocalDiskCache : public CacheStorage {
public:
  explicit LocalDiskCache(ir::StringRef Dir, size_t MaxSize);
  ~LocalDiskCache() override;

  LocalDiskCache(const LocalDiskCache&) = delete;
  LocalDiskCache& operator=(const LocalDiskCache&) = delete;

  std::optional<CacheEntry> lookup(const CacheKey& Key) override;
  bool store(const CacheKey& Key, const CacheEntry& Entry) override;
  Stats getStats() const override;
  void evictIfNeeded();

  ir::StringRef getCacheDir() const;
  size_t getMaxSize() const;

private:
  class Impl;
  Impl* Pimpl;
};

// ============================================================
// RemoteCache — 远程缓存（stub）
// ============================================================

class RemoteCache : public CacheStorage {
public:
  explicit RemoteCache(ir::StringRef Endpoint, ir::StringRef Bucket);
  std::optional<CacheEntry> lookup(const CacheKey& Key) override;
  bool store(const CacheKey& Key, const CacheEntry& Entry) override;
  Stats getStats() const override;
private:
  std::string Endpoint_;
  std::string Bucket_;
};

// ============================================================
// CompilationCacheManager — 编译缓存管理器
// ============================================================

class CompilationCacheManager {
public:
  CompilationCacheManager();
  ~CompilationCacheManager();

  CompilationCacheManager(const CompilationCacheManager&) = delete;
  CompilationCacheManager& operator=(const CompilationCacheManager&) = delete;

  void enable(ir::StringRef CacheDir, size_t MaxSize);
  void disable();
  bool isEnabled() const;

  /// stub — 返回 nullopt
  std::optional<std::unique_ptr<ir::IRModule>>
  lookupIR(const CacheKey& Key, ir::IRTypeContext& Ctx);

  std::optional<std::vector<uint8_t>> lookupObject(const CacheKey& Key);

  /// stub — 返回 false
  bool storeIR(const CacheKey& Key, const ir::IRModule& M);

  bool storeObject(const CacheKey& Key, ir::ArrayRef<uint8_t> Data);

private:
  std::unique_ptr<CacheStorage> Storage_;
  bool Enabled_ = false;
};

} // namespace cache
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCACHE_H
