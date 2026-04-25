# Task A-F7 优化版：CacheKey/CacheEntry 基础类型定义 + 编译器缓存架构

> 文档版本：v1.0 | 由 planner 产出

---

## 一、任务概述

| 项目 | 内容 |
|------|------|
| 任务编号 | A-F7 |
| 任务名称 | CacheKey/CacheEntry 基础类型定义 + 编译器缓存架构 |
| 依赖 | A.3.1（IRFormatVersion）— ✅ 已存在 |
| 对现有代码的影响 | 仅 `src/IR/CMakeLists.txt` 和 `tests/unit/IR/CMakeLists.txt` 需追加条目 |

**产出文件**

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRCache.h` |
| 新增 | `src/IR/IRCache.cpp` |
| 新增 | `tests/unit/IR/IRCacheTest.cpp` |
| 修改 | `src/IR/CMakeLists.txt`（+1 行） |
| 修改 | `tests/unit/IR/CMakeLists.txt`（+1 行） |

---

## 二、代码背景分析 — 8 个关键设计决策

### 决策 1：CompilerOptions 不存在 → 定义简化版 `CacheOptions`

全项目搜索确认无 `CompilerOptions` 或 `CacheOptions`。`CacheKey` 的 5 个 hash 字段明确需要：源码、编译选项、版本、目标三元组、依赖。

**决策**：定义 `cache::CacheOptions`，仅含缓存键计算所需的字段子集。

```cpp
struct CacheOptions {
  std::string TargetTriple;
  std::string DataLayout;
  uint64_t FeatureFlags = 0;
  ir::IRFormatVersion Version = ir::IRFormatVersion::Current();
};
```

理由：`CompilerOptions` 是驱动层概念，不属于 IR 层。`CacheOptions` 仅提取影响缓存的字段，未来可通过 `fromCompilerOptions()` 转换。

### 决策 2：Optional → `std::optional<T>`

A-F5/A-F6 已统一使用 `std::optional<T>`。保持一致。

### 决策 3：CompilationCacheManager::lookupIR/storeIR → stub

`lookupIR` 需 IRModule 反序列化（依赖 IRSerializer）。A-F7 是基础定义阶段。

**决策**：声明完整接口，stub 实现（`lookupIR` 返回 `nullopt`，`storeIR` 返回 `false`）。`LocalDiskCache` 的字节级 `lookup`/`store` 提供完整实现。

理由：符合渐进式改造红线。LocalDiskCache 的字节级操作不依赖 IRModule。

### 决策 4：BLAKE3 → FNV-1a 64-bit 初始实现

项目无 BLAKE3。

**决策**：使用 FNV-1a 64-bit，封装在 `computeHash()` 函数中，后续可替换。

理由：Phase A 不引入第三方库。FNV-1a ~15 行无依赖。`CacheKey` 对外只暴露 `CombinedHash`/`toHex()`，算法是纯实现细节。

### 决策 5：LocalDiskCache → 完整实现

目录创建（`std::filesystem`）、文件读写、简单总大小限制淘汰。~150 行可控，使 V2 验收标准可直接测试。

### 决策 6：RemoteCache → stub

声明接口，`lookup` 返回 `nullopt`，`store` 返回 `false`。spec 注明"远期实现"。

### 决策 7：命名空间 → `blocktype::cache`

与 `blocktype::diag`（A-F6）一致。文件放 `IR/` 目录是物理组织便利。

### 决策 8：IRModule 依赖 → 前向声明

`CompilationCacheManager` 头文件前向声明 `ir::IRModule` 和 `ir::IRTypeContext`，不 include 完整定义。

---

## 三、完整头文件 — `include/blocktype/IR/IRCache.h`

```cpp
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
```

---

## 四、完整实现文件 — `src/IR/IRCache.cpp`

```cpp
#include "blocktype/IR/IRCache.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace blocktype {
namespace cache {

// ============================================================
// FNV-1a 64-bit 哈希（后续可替换为 BLAKE3）
// ============================================================

static uint64_t fnv1a64(const void* Data, size_t Len) {
  auto* B = static_cast<const uint8_t*>(Data);
  uint64_t H = 14695981039346656037ULL;
  for (size_t i = 0; i < Len; ++i) { H ^= B[i]; H *= 1099511628211ULL; }
  return H;
}

static uint64_t hashStr(ir::StringRef S) { return fnv1a64(S.data(), S.size()); }

static uint64_t hashU64(uint64_t V) { return fnv1a64(&V, sizeof(V)); }

static uint64_t combineHash(uint64_t A, uint64_t B) {
  return A ^ (B * 1099511628211ULL + 0x9e3779b97f4a7c15ULL);
}

// ============================================================
// CacheKey
// ============================================================

CacheKey CacheKey::compute(ir::StringRef Source, const CacheOptions& Opts) {
  CacheKey K;
  K.SourceHash = hashStr(Source);
  K.OptionsHash = hashU64(Opts.FeatureFlags) ^ hashStr(ir::StringRef(Opts.DataLayout));
  K.VersionHash = hashU64((uint64_t)Opts.Version.Major << 32 |
                           (uint64_t)Opts.Version.Minor << 16 |
                           (uint64_t)Opts.Version.Patch);
  K.TargetTripleHash = hashStr(ir::StringRef(Opts.TargetTriple));
  K.DependencyHash = 0;
  K.CombinedHash = combineHash(
    combineHash(K.SourceHash, K.OptionsHash),
    combineHash(combineHash(K.VersionHash, K.TargetTripleHash), K.DependencyHash));
  return K;
}

std::string CacheKey::toHex() const {
  char Buf[17];
  std::snprintf(Buf, sizeof(Buf), "%016llx", (unsigned long long)CombinedHash);
  return Buf;
}

std::string CacheKey::toPathComponents() const {
  std::string H = toHex();
  return H.substr(0, 2) + "/" + H.substr(2);
}

// ============================================================
// CacheEntry JSON
// ============================================================

std::string CacheEntry::toJSON() const {
  std::string S;
  S += "{\n  \"combined_hash\": \""; S += Key.toHex(); S += "\",\n";
  S += "  \"ir_version_major\": "; S += std::to_string(IRVersion.Major); S += ",\n";
  S += "  \"ir_version_minor\": "; S += std::to_string(IRVersion.Minor); S += ",\n";
  S += "  \"ir_version_patch\": "; S += std::to_string(IRVersion.Patch); S += ",\n";
  S += "  \"timestamp\": "; S += std::to_string(Timestamp); S += ",\n";
  S += "  \"ir_size\": "; S += std::to_string(IRSize); S += ",\n";
  S += "  \"object_size\": "; S += std::to_string(ObjectSize); S += "\n}\n";
  return S;
}

static std::string extractJSONValue(const std::string& JSON, const std::string& Key) {
  auto Pos = JSON.find(Key);
  if (Pos == std::string::npos) return "";
  Pos = JSON.find(':', Pos + Key.size());
  if (Pos == std::string::npos) return "";
  ++Pos;
  while (Pos < JSON.size() && (JSON[Pos]==' '||JSON[Pos]=='\t')) ++Pos;
  if (Pos < JSON.size() && JSON[Pos] == '"') {
    auto End = JSON.find('"', ++Pos);
    return JSON.substr(Pos, End - Pos);
  }
  auto End = JSON.find_first_of(",}\n", Pos);
  if (End == std::string::npos) End = JSON.size();
  auto Val = JSON.substr(Pos, End - Pos);
  while (!Val.empty() && (Val.back()==' '||Val.back()=='\t')) Val.pop_back();
  return Val;
}

std::optional<CacheEntry> CacheEntry::fromJSON(const std::string& JSON) {
  CacheEntry E;
  auto H = extractJSONValue(JSON, "\"combined_hash\"");
  if (!H.empty()) E.Key.CombinedHash = std::stoull(H, nullptr, 16);
  auto M = extractJSONValue(JSON, "\"ir_version_major\"");
  if (!M.empty()) E.IRVersion.Major = (uint16_t)std::stoul(M);
  auto N = extractJSONValue(JSON, "\"ir_version_minor\"");
  if (!N.empty()) E.IRVersion.Minor = (uint16_t)std::stoul(N);
  auto P = extractJSONValue(JSON, "\"ir_version_patch\"");
  if (!P.empty()) E.IRVersion.Patch = (uint16_t)std::stoul(P);
  auto T = extractJSONValue(JSON, "\"timestamp\"");
  if (!T.empty()) E.Timestamp = std::stoull(T);
  auto I = extractJSONValue(JSON, "\"ir_size\"");
  if (!I.empty()) E.IRSize = std::stoull(I);
  auto O = extractJSONValue(JSON, "\"object_size\"");
  if (!O.empty()) E.ObjectSize = std::stoull(O);
  return E;
}

// ============================================================
// LocalDiskCache
// ============================================================

class LocalDiskCache::Impl {
public:
  std::string CacheDir;
  size_t MaxSize;
  mutable size_t Hits = 0, Misses = 0;
  size_t Evictions = 0;

  Impl(ir::StringRef D, size_t M) : CacheDir(D.str()), MaxSize(M) {}
  std::string entryDir(const CacheKey& K) const { return CacheDir + "/" + K.toPathComponents(); }

  static bool ensureDir(const std::string& P) {
    std::error_code EC; std::filesystem::create_directories(P, EC); return !EC;
  }
  static bool readFile(const std::string& P, std::vector<uint8_t>& Out) {
    std::ifstream F(P, std::ios::binary|std::ios::ate);
    if (!F) return false;
    auto Sz = F.tellg(); if (Sz <= 0) return false;
    F.seekg(0); Out.resize((size_t)Sz);
    F.read((char*)Out.data(), Sz); return F.good();
  }
  static bool writeFile(const std::string& P, const uint8_t* D, size_t L) {
    std::ofstream F(P, std::ios::binary);
    if (!F) return false;
    F.write((const char*)D, (std::streamsize)L); return F.good();
  }
  size_t computeTotalSize() const {
    size_t T = 0; std::error_code EC;
    for (auto& E : std::filesystem::recursive_directory_iterator(CacheDir, EC))
      if (E.is_regular_file()) T += (size_t)E.file_size();
    return T;
  }
};

LocalDiskCache::LocalDiskCache(ir::StringRef Dir, size_t Max)
    : Pimpl(new Impl(Dir, Max)) { Impl::ensureDir(Pimpl->CacheDir); }
LocalDiskCache::~LocalDiskCache() { delete Pimpl; }

std::optional<CacheEntry> LocalDiskCache::lookup(const CacheKey& Key) {
  std::string Dir = Pimpl->entryDir(Key);
  std::vector<uint8_t> Meta;
  if (!Impl::readFile(Dir + "/meta.json", Meta)) { ++Pimpl->Misses; return std::nullopt; }
  auto Entry = CacheEntry::fromJSON(std::string(Meta.begin(), Meta.end()));
  if (!Entry) { ++Pimpl->Misses; return std::nullopt; }
  Entry->Key = Key;
  Impl::readFile(Dir + "/ir.btir", Entry->IRData);
  Impl::readFile(Dir + "/obj.o", Entry->ObjectData);
  ++Pimpl->Hits;
  return Entry;
}

bool LocalDiskCache::store(const CacheKey& Key, const CacheEntry& Entry) {
  std::string Dir = Pimpl->entryDir(Key);
  if (!Impl::ensureDir(Dir)) return false;
  if (!Entry.IRData.empty())
    if (!Impl::writeFile(Dir + "/ir.btir", Entry.IRData.data(), Entry.IRData.size())) return false;
  if (!Entry.ObjectData.empty())
    if (!Impl::writeFile(Dir + "/obj.o", Entry.ObjectData.data(), Entry.ObjectData.size())) return false;
  auto M = Entry.toJSON();
  if (!Impl::writeFile(Dir + "/meta.json", (const uint8_t*)M.data(), M.size())) return false;
  evictIfNeeded();
  return true;
}

CacheStorage::Stats LocalDiskCache::getStats() const {
  return {Pimpl->Hits, Pimpl->Misses, Pimpl->Evictions, Pimpl->computeTotalSize()};
}

void LocalDiskCache::evictIfNeeded() {
  if (Pimpl->computeTotalSize() <= Pimpl->MaxSize) return;
  std::vector<std::pair<uint64_t,std::string>> Entries;
  std::error_code EC;
  for (auto& D1 : std::filesystem::directory_iterator(Pimpl->CacheDir, EC)) {
    if (!D1.is_directory()) continue;
    for (auto& D2 : std::filesystem::directory_iterator(D1.path(), EC)) {
      if (!D2.is_directory()) continue;
      std::vector<uint8_t> M;
      if (Impl::readFile(D2.path().string()+"/meta.json", M)) {
        auto E = CacheEntry::fromJSON(std::string(M.begin(),M.end()));
        if (E) Entries.emplace_back(E->Timestamp, D2.path().string());
      }
    }
  }
  std::sort(Entries.begin(), Entries.end());
  for (auto& [/*Ts*/,Path] : Entries) {
    if (Pimpl->computeTotalSize() <= Pimpl->MaxSize) break;
    std::error_code RmEC; std::filesystem::remove_all(Path, RmEC);
    ++Pimpl->Evictions;
  }
}

ir::StringRef LocalDiskCache::getCacheDir() const { return Pimpl->CacheDir; }
size_t LocalDiskCache::getMaxSize() const { return Pimpl->MaxSize; }

// ============================================================
// RemoteCache — stub
// ============================================================

RemoteCache::RemoteCache(ir::StringRef Ep, ir::StringRef Bk)
    : Endpoint_(Ep.str()), Bucket_(Bk.str()) {}
std::optional<CacheEntry> RemoteCache::lookup(const CacheKey&) { return std::nullopt; }
bool RemoteCache::store(const CacheKey&, const CacheEntry&) { return false; }
CacheStorage::Stats RemoteCache::getStats() const { return {}; }

// ============================================================
// CompilationCacheManager
// ============================================================

CompilationCacheManager::CompilationCacheManager() = default;
CompilationCacheManager::~CompilationCacheManager() = default;

void CompilationCacheManager::enable(ir::StringRef D, size_t M) {
  Storage_ = std::make_unique<LocalDiskCache>(D, M); Enabled_ = true;
}
void CompilationCacheManager::disable() { Storage_.reset(); Enabled_ = false; }
bool CompilationCacheManager::isEnabled() const { return Enabled_; }

std::optional<std::unique_ptr<ir::IRModule>>
CompilationCacheManager::lookupIR(const CacheKey&, ir::IRTypeContext&) {
  // TODO: 用 IRReader::parseBitcode 反序列化
  return std::nullopt;
}

std::optional<std::vector<uint8_t>>
CompilationCacheManager::lookupObject(const CacheKey& Key) {
  if (!Enabled_ || !Storage_) return std::nullopt;
  auto E = Storage_->lookup(Key);
  if (!E || E->ObjectData.empty()) return std::nullopt;
  return std::move(E->ObjectData);
}

bool CompilationCacheManager::storeIR(const CacheKey&, const ir::IRModule&) {
  // TODO: 用 IRWriter::writeBitcode 序列化
  return false;
}

bool CompilationCacheManager::storeObject(const CacheKey& Key, ir::ArrayRef<uint8_t> Data) {
  if (!Enabled_ || !Storage_) return false;
  CacheEntry E; E.Key = Key;
  E.ObjectData.assign(Data.begin(), Data.end());
  E.ObjectSize = Data.size();
  E.Timestamp = (uint64_t)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  return Storage_->store(Key, E);
}

} // namespace cache
} // namespace blocktype
```

---

## 五、完整测试文件 — `tests/unit/IR/IRCacheTest.cpp`

```cpp
#include <gtest/gtest.h>
#include <filesystem>
#include "blocktype/IR/IRCache.h"

using namespace blocktype;
using namespace blocktype::cache;

static std::string makeTempDir() {
  auto D = std::filesystem::temp_directory_path() / ("btcache_" + std::to_string(::getpid()));
  std::filesystem::create_directories(D);
  return D.string();
}
static void rmDir(const std::string& D) { std::error_code EC; std::filesystem::remove_all(D,EC); }

// V1: CacheKey 计算
TEST(IRCacheTest, CacheKeyCompute) {
  CacheOptions Opts; Opts.TargetTriple = "x86_64-linux-gnu";
  auto K = CacheKey::compute("int main(){}", Opts);
  EXPECT_NE(K.CombinedHash, 0ull);
  EXPECT_NE(K.SourceHash, 0ull);
}

// V2: CacheKey 确定性
TEST(IRCacheTest, CacheKeyDeterministic) {
  CacheOptions O;
  EXPECT_EQ(CacheKey::compute("src", O).CombinedHash, CacheKey::compute("src", O).CombinedHash);
}

// V3: 不同源码不同 key
TEST(IRCacheTest, CacheKeyDiffersOnSource) {
  CacheOptions O;
  EXPECT_NE(CacheKey::compute("a", O).CombinedHash, CacheKey::compute("b", O).CombinedHash);
}

// V4: 不同选项不同 key
TEST(IRCacheTest, CacheKeyDiffersOnOptions) {
  CacheOptions O1; O1.TargetTriple = "x86_64-linux";
  CacheOptions O2; O2.TargetTriple = "aarch64-macos";
  EXPECT_NE(CacheKey::compute("s", O1).CombinedHash, CacheKey::compute("s", O2).CombinedHash);
}

// V5: toHex 格式
TEST(IRCacheTest, CacheKeyToHex) {
  CacheOptions O;
  auto H = CacheKey::compute("x", O).toHex();
  EXPECT_EQ(H.size(), 16u);
}

// V6: toPathComponents 格式
TEST(IRCacheTest, CacheKeyPathComponents) {
  CacheOptions O;
  auto P = CacheKey::compute("x", O).toPathComponents();
  EXPECT_EQ(P.size(), 19u);
  EXPECT_EQ(P[2], '/');
}

// V7: LocalDiskCache 存取
TEST(IRCacheTest, LocalDiskCacheStoreLookup) {
  std::string D = makeTempDir();
  LocalDiskCache LDC(D, 1ull<<30);
  CacheOptions O;
  auto K = CacheKey::compute("int main(){}", O);
  CacheEntry E; E.Key = K; E.IRData = {1,2,3}; E.IRSize = 3;
  ASSERT_TRUE(LDC.store(K, E));
  auto F = LDC.lookup(K);
  ASSERT_TRUE(F.has_value());
  EXPECT_EQ(F->IRData.size(), 3u);
  EXPECT_EQ(F->IRData[0], 1);
  rmDir(D);
}

// V8: LocalDiskCache miss
TEST(IRCacheTest, LocalDiskCacheMiss) {
  std::string D = makeTempDir();
  LocalDiskCache LDC(D, 1<<20);
  CacheOptions O;
  EXPECT_FALSE(LDC.lookup(CacheKey::compute("nope",O)).has_value());
  EXPECT_EQ(LDC.getStats().Misses, 1u);
  rmDir(D);
}

// V9: LocalDiskCache 统计
TEST(IRCacheTest, LocalDiskCacheStats) {
  std::string D = makeTempDir();
  LocalDiskCache LDC(D, 1<<20);
  CacheOptions O; auto K = CacheKey::compute("stats",O);
  CacheEntry E; E.Key = K; E.ObjectData = {0xAA}; E.ObjectSize = 1;
  LDC.store(K, E);
  LDC.lookup(K); LDC.lookup(K);
  auto S = LDC.getStats();
  EXPECT_EQ(S.Hits, 2u);
  EXPECT_GT(S.TotalSize, 0u);
  rmDir(D);
}

// V10: CacheEntry JSON 序列化/反序列化
TEST(IRCacheTest, CacheEntryJSON) {
  CacheEntry E; E.Key.CombinedHash = 0xdeadbeef12345678ULL;
  E.IRVersion = {1,2,3}; E.Timestamp = 9999; E.IRSize = 100; E.ObjectSize = 200;
  auto J = E.toJSON();
  EXPECT_NE(J.find("deadbeef12345678"), std::string::npos);
  EXPECT_NE(J.find("\"ir_version_major\": 1"), std::string::npos);
  auto P = CacheEntry::fromJSON(J);
  ASSERT_TRUE(P.has_value());
  EXPECT_EQ(P->Key.CombinedHash, 0xdeadbeef12345678ULL);
  EXPECT_EQ(P->IRVersion.Major, 1u);
  EXPECT_EQ(P->Timestamp, 9999ull);
}

// V11: CompilationCacheManager enable/disable
TEST(IRCacheTest, ManagerEnableDisable) {
  CompilationCacheManager Mgr;
  EXPECT_FALSE(Mgr.isEnabled());
  std::string D = makeTempDir();
  Mgr.enable(D, 1<<20);
  EXPECT_TRUE(Mgr.isEnabled());
  Mgr.disable();
  EXPECT_FALSE(Mgr.isEnabled());
  rmDir(D);
}

// V12: CompilationCacheManager storeObject/lookupObject
TEST(IRCacheTest, ManagerObjectStoreLookup) {
  std::string D = makeTempDir();
  CompilationCacheManager Mgr;
  Mgr.enable(D, 1<<20);
  CacheOptions O; auto K = CacheKey::compute("obj_test", O);
  std::vector<uint8_t> Data = {1,2,3,4,5};
  ASSERT_TRUE(Mgr.storeObject(K, ir::ArrayRef<uint8_t>(Data)));
  auto Found = Mgr.lookupObject(K);
  ASSERT_TRUE(Found.has_value());
  EXPECT_EQ(Found->size(), 5u);
  Mgr.disable();
  rmDir(D);
}

// V13: CompilationCacheManager lookupIR stub
TEST(IRCacheTest, ManagerLookupIRStub) {
  std::string D = makeTempDir();
  CompilationCacheManager Mgr;
  Mgr.enable(D, 1<<20);
  CacheOptions O; auto K = CacheKey::compute("ir_test", O);
  ir::IRTypeContext Ctx;
  EXPECT_FALSE(Mgr.lookupIR(K, Ctx).has_value());
  Mgr.disable();
  rmDir(D);
}

// V14: CompilationCacheManager storeIR stub
TEST(IRCacheTest, ManagerStoreIRStub) {
  std::string D = makeTempDir();
  CompilationCacheManager Mgr;
  Mgr.enable(D, 1<<20);
  CacheOptions O; auto K = CacheKey::compute("ir_store", O);
  ir::IRTypeContext Ctx;
  ir::IRModule M("test", Ctx);
  EXPECT_FALSE(Mgr.storeIR(K, M));
  Mgr.disable();
  rmDir(D);
}

// V15: CacheOptions 默认版本
TEST(IRCacheTest, CacheOptionsDefaultVersion) {
  CacheOptions O;
  EXPECT_EQ(O.Version.Major, ir::IRFormatVersion::Current().Major);
  EXPECT_EQ(O.Version.Minor, ir::IRFormatVersion::Current().Minor);
}
```

---

## 六、CMakeLists.txt 修改

### `src/IR/CMakeLists.txt` — 在列表末尾追加：

```cmake
  IRCache.cpp
```

### `tests/unit/IR/CMakeLists.txt` — 在列表末尾追加：

```cmake
  IRCacheTest.cpp
```

---

## 七、验收标准映射

| 编号 | 验收内容 | 对应测试 |
|------|----------|----------|
| V1 | CacheKey 计算 CombinedHash ≠ 0 | `CacheKeyCompute` |
| V2 | CacheKey 确定性 | `CacheKeyDeterministic` |
| V3 | 不同源码不同 key | `CacheKeyDiffersOnSource` |
| V4 | 不同选项不同 key | `CacheKeyDiffersOnOptions` |
| V5 | toHex 16 字符 | `CacheKeyToHex` |
| V6 | toPathComponents "ab/rest" 格式 | `CacheKeyPathComponents` |
| V7 | LocalDiskCache 存取 | `LocalDiskCacheStoreLookup` |
| V8 | LocalDiskCache miss | `LocalDiskCacheMiss` |
| V9 | LocalDiskCache 统计 | `LocalDiskCacheStats` |
| V10 | CacheEntry JSON 序列化/反序列化 | `CacheEntryJSON` |
| V11 | Manager enable/disable | `ManagerEnableDisable` |
| V12 | Manager storeObject/lookupObject | `ManagerObjectStoreLookup` |
| V13 | Manager lookupIR stub | `ManagerLookupIRStub` |
| V14 | Manager storeIR stub | `ManagerStoreIRStub` |
| V15 | CacheOptions 默认版本正确 | `CacheOptionsDefaultVersion` |

---

## 八、sizeof 影响评估

| 类型 | 大小 | 说明 |
|------|------|------|
| CacheKey | 48 bytes | 6 × uint64_t |
| CacheOptions | ~80 bytes | 2 × string + uint64_t + IRFormatVersion(6) |
| CacheEntry | ~120 bytes + 数据 | 含两个 vector（堆分配） |
| LocalDiskCache | ~指针 | Pimpl，实际数据在堆上 |

---

## 九、风险与注意事项

### 9.1 `std::filesystem` C++17 依赖

项目 CMakeLists.txt 使用 C++23 标准（`set(CMAKE_CXX_STANDARD 23)`），`<filesystem>` 完全可用。

### 9.2 JSON 解析简易性

`CacheEntry::fromJSON` 使用手工解析，仅支持固定格式。足够元数据场景，不引入第三方 JSON 库。

### 9.3 FNV-1a 碰撞风险

64-bit FNV-1a 碰撞概率极低（~2^-32）。缓存 miss 仅导致重新编译，不会产生错误结果。后续替换 BLAKE3 时接口零改动。

### 9.4 红线 Checklist 确认

| # | 红线 | 状态 |
|---|------|------|
| (1) | 架构优先 — 缓存独立于前后端 | ✅ `cache` 独立命名空间 |
| (2) | 多前端多后端自由组合 | ✅ 纯新增文件，无耦合 |
| (3) | 渐进式改造 | ✅ lookupIR/storeIR 为 stub |
| (4) | 现有功能不退化 | ✅ 不修改任何现有接口 |
| (5) | 接口抽象优先 | ✅ CacheStorage 抽象基类 |
| (6) | IR 中间层解耦 | ✅ 缓存属于 IR 层基础设施 |

---

## 十、实现步骤

| 步骤 | 操作 |
|------|------|
| 1 | 创建 `include/blocktype/IR/IRCache.h` |
| 2 | 创建 `src/IR/IRCache.cpp` |
| 3 | 修改 `src/IR/CMakeLists.txt`（+`IRCache.cpp`） |
| 4 | 修改 `tests/unit/IR/CMakeLists.txt`（+`IRCacheTest.cpp`） |
| 5 | 创建 `tests/unit/IR/IRCacheTest.cpp` |
| 6 | 编译 `blocktype-ir`，确认无错误 |
| 7 | 运行测试 `blocktype-ir-test`，15 个测试全部通过 |
| 8 | 运行全量 `ctest`，确认无退化 |
| 9 | Git commit |
