# AI Coder 可执行任务流 — Phase B 增强任务 B-F8：LocalDiskCache 基础实现

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype::cache` 中
5. **头文件路径**：`include/blocktype/IR/`，源文件路径：`src/IR/Cache/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F8 — LocalDiskCache 基础实现"
   git push origin HEAD
   ```

---

## Task B-F8：LocalDiskCache 基础实现

### 依赖验证

✅ **A-F7（CacheKey/CacheEntry 基础类型）**：已存在于 `include/blocktype/IR/IRCache.h`

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `src/IR/Cache/LocalDiskCache.cpp` |
| 新增 | `src/IR/Cache/CMakeLists.txt` |

**注意**：`LocalDiskCache` 类已在 `IRCache.h` 中声明（Pimpl 模式），此处只需实现。

---

## 必须实现的类型定义

### LocalDiskCache::Impl 实现

```cpp
// 在 src/IR/Cache/LocalDiskCache.cpp 中实现

#include "blocktype/IR/IRCache.h"
#include "blocktype/IR/ADT.h"
#include <fstream>
#include <filesystem>
#include <sys/stat.h>

namespace blocktype {
namespace cache {

namespace fs = std::filesystem;

class LocalDiskCache::Impl {
  std::string CacheDir;
  size_t MaxSize;
  mutable size_t CurrentSize;
  mutable size_t Hits;
  mutable size_t Misses;
  mutable size_t Evictions;
  
public:
  Impl(StringRef Dir, size_t Max)
    : CacheDir(Dir.str()), MaxSize(Max), CurrentSize(0),
      Hits(0), Misses(0), Evictions(0) {
    // 创建缓存目录
    fs::create_directories(CacheDir);
    
    // 计算当前缓存大小
    computeCurrentSize();
  }
  
  std::optional<CacheEntry> lookup(const CacheKey& Key) {
    std::string Path = getCachePath(Key);
    
    if (!fs::exists(Path)) {
      ++Misses;
      return std::nullopt;
    }
    
    // 读取元数据
    std::string MetaPath = Path + "/meta.json";
    std::ifstream MetaFile(MetaPath);
    if (!MetaFile) {
      ++Misses;
      return std::nullopt;
    }
    
    std::string MetaContent((std::istreambuf_iterator<char>(MetaFile)),
                             std::istreambuf_iterator<char>());
    
    auto Entry = CacheEntry::fromJSON(MetaContent);
    if (!Entry) {
      ++Misses;
      return std::nullopt;
    }
    
    // 读取 IR 数据
    std::string IRPath = Path + "/ir.btir";
    if (fs::exists(IRPath)) {
      std::ifstream IRFile(IRPath, std::ios::binary);
      Entry->IRData = std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(IRFile)),
        std::istreambuf_iterator<char>()
      );
    }
    
    // 读取目标文件数据
    std::string ObjPath = Path + "/obj.o";
    if (fs::exists(ObjPath)) {
      std::ifstream ObjFile(ObjPath, std::ios::binary);
      Entry->ObjectData = std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(ObjFile)),
        std::istreambuf_iterator<char>()
      );
    }
    
    ++Hits;
    return Entry;
  }
  
  bool store(const CacheKey& Key, const CacheEntry& Entry) {
    std::string Path = getCachePath(Key);
    
    // 创建缓存子目录
    fs::create_directories(Path);
    
    // 写入元数据
    std::string MetaPath = Path + "/meta.json";
    std::ofstream MetaFile(MetaPath);
    if (!MetaFile) return false;
    MetaFile << Entry.toJSON();
    MetaFile.close();
    
    // 写入 IR 数据
    if (!Entry.IRData.empty()) {
      std::string IRPath = Path + "/ir.btir";
      std::ofstream IRFile(IRPath, std::ios::binary);
      if (!IRFile) return false;
      IRFile.write(reinterpret_cast<const char*>(Entry.IRData.data()),
                   Entry.IRData.size());
      IRFile.close();
    }
    
    // 写入目标文件数据
    if (!Entry.ObjectData.empty()) {
      std::string ObjPath = Path + "/obj.o";
      std::ofstream ObjFile(ObjPath, std::ios::binary);
      if (!ObjFile) return false;
      ObjFile.write(reinterpret_cast<const char*>(Entry.ObjectData.data()),
                    Entry.ObjectData.size());
      ObjFile.close();
    }
    
    // 更新当前大小
    CurrentSize += Entry.IRData.size() + Entry.ObjectData.size();
    
    // 检查是否需要淘汰
    evictIfNeeded();
    
    return true;
  }
  
  CacheStorage::Stats getStats() const {
    return {Hits, Misses, Evictions, CurrentSize};
  }
  
  void evictIfNeeded() {
    if (CurrentSize <= MaxSize) return;
    
    // LRU 淘汰策略：删除最旧的缓存文件
    std::vector<std::pair<std::string, uint64_t>> Entries;
    
    for (const auto& Entry : fs::directory_iterator(CacheDir)) {
      if (!Entry.is_directory()) continue;
      
      // 递归查找所有缓存条目
      for (const auto& SubEntry : fs::directory_iterator(Entry.path())) {
        if (!SubEntry.is_directory()) continue;
        
        std::string MetaPath = SubEntry.path().string() + "/meta.json";
        if (!fs::exists(MetaPath)) continue;
        
        // 获取修改时间
        auto MTime = fs::last_write_time(MetaPath);
        auto Timestamp = std::chrono::duration_cast<std::chrono::seconds>(
          MTime.time_since_epoch()
        ).count();
        
        Entries.push_back({SubEntry.path().string(), Timestamp});
      }
    }
    
    // 按时间戳排序（最旧的在前）
    std::sort(Entries.begin(), Entries.end(),
              [](const auto& A, const auto& B) { return A.second < B.second; });
    
    // 淘汰直到满足大小限制
    for (const auto& [Path, _] : Entries) {
      if (CurrentSize <= MaxSize * 0.9) break;  // 淘汰到 90%
      
      size_t Size = computeEntrySize(Path);
      fs::remove_all(Path);
      CurrentSize -= Size;
      ++Evictions;
    }
  }
  
  StringRef getCacheDir() const { return CacheDir; }
  size_t getMaxSize() const { return MaxSize; }
  
private:
  std::string getCachePath(const CacheKey& Key) const {
    // 目录结构：<CacheDir>/<first2hex>/<rest38hex>/
    std::string Hex = Key.toHex();
    return CacheDir + "/" + Hex.substr(0, 2) + "/" + Hex.substr(2);
  }
  
  void computeCurrentSize() {
    CurrentSize = 0;
    for (const auto& Entry : fs::recursive_directory_iterator(CacheDir)) {
      if (Entry.is_regular_file()) {
        CurrentSize += fs::file_size(Entry);
      }
    }
  }
  
  size_t computeEntrySize(const std::string& Path) const {
    size_t Size = 0;
    for (const auto& Entry : fs::directory_iterator(Path)) {
      if (Entry.is_regular_file()) {
        Size += fs::file_size(Entry);
      }
    }
    return Size;
  }
};

// LocalDiskCache 实现（Pimpl 模式）

LocalDiskCache::LocalDiskCache(StringRef Dir, size_t Max)
  : Pimpl(new Impl(Dir, Max)) {}

LocalDiskCache::~LocalDiskCache() {
  delete Pimpl;
}

std::optional<CacheEntry> LocalDiskCache::lookup(const CacheKey& Key) {
  return Pimpl->lookup(Key);
}

bool LocalDiskCache::store(const CacheKey& Key, const CacheEntry& Entry) {
  return Pimpl->store(Key, Entry);
}

CacheStorage::Stats LocalDiskCache::getStats() const {
  return Pimpl->getStats();
}

void LocalDiskCache::evictIfNeeded() {
  Pimpl->evictIfNeeded();
}

StringRef LocalDiskCache::getCacheDir() const {
  return Pimpl->getCacheDir();
}

size_t LocalDiskCache::getMaxSize() const {
  return Pimpl->getMaxSize();
}

} // namespace cache
} // namespace blocktype
```

---

## 缓存文件结构

```
<CacheDir>/
  ab/                      # 前 2 位 hex
    cdef0123456789.../     # 后 38 位 hex
      meta.json            # 元数据（JSON 格式）
      ir.btir              # IR 缓存（BTIR 二进制格式）
      obj.o                # 目标文件缓存
  cd/
    ...
```

### meta.json 格式

```json
{
  "key": {
    "source_hash": "0xabc123...",
    "options_hash": "0xdef456...",
    "version_hash": "0x123789...",
    "target_triple_hash": "0x456abc...",
    "dependency_hash": "0x789def...",
    "combined_hash": "0xabcdef..."
  },
  "ir_version": "1.0.0",
  "timestamp": 1234567890,
  "ir_size": 1024,
  "object_size": 2048
}
```

---

## CacheKey 计算规则

```cpp
CacheKey CacheKey::compute(StringRef Source, const CacheOptions& Opts) {
  CacheKey Key;
  
  // 1. 源码哈希（BLAKE3）
  Key.SourceHash = computeBLAKE3Hash(Source);
  
  // 2. 编译选项哈希
  std::string OptsStr;
  OptsStr += Opts.TargetTriple;
  OptsStr += Opts.DataLayout;
  OptsStr += std::to_string(Opts.FeatureFlags);
  Key.OptionsHash = computeBLAKE3Hash(OptsStr);
  
  // 3. 版本哈希
  Key.VersionHash = computeBLAKE3Hash(Opts.Version.toString());
  
  // 4. 目标三元组哈希
  Key.TargetTripleHash = computeBLAKE3Hash(Opts.TargetTriple);
  
  // 5. 依赖哈希（远期：包含所有 #include 文件的哈希）
  Key.DependencyHash = 0;  // 当前 stub
  
  // 6. 组合哈希
  std::string Combined;
  Combined += std::to_string(Key.SourceHash);
  Combined += std::to_string(Key.OptionsHash);
  Combined += std::to_string(Key.VersionHash);
  Combined += std::to_string(Key.TargetTripleHash);
  Combined += std::to_string(Key.DependencyHash);
  Key.CombinedHash = computeBLAKE3Hash(Combined);
  
  return Key;
}
```

---

## 编译选项支持

```bash
--fcache-dir=<path>           指定编译缓存目录（默认：$HOME/.blocktype/cache）
--fcache-size=<size>          最大缓存大小（如 "10G"，默认：5G）
--fcache-no                   禁用编译缓存
--fcache-stats                输出缓存统计
```

### 选项解析

```cpp
// 在 CompilerInvocation 中添加

if (Arg *A = Args.getLastArg(OPT_fcache_dir)) {
  Opts.CacheDir = A->getValue();
}

if (Arg *A = Args.getLastArg(OPT_fcache_size)) {
  Opts.CacheMaxSize = parseSize(A->getValue());  // "10G" → 10*1024*1024*1024
}

if (Args.hasArg(OPT_fcache_no)) {
  Opts.CacheEnabled = false;
}

if (Args.hasArg(OPT_fcache_stats)) {
  Opts.CacheStats = true;
}
```

---

## 实现约束

1. **BLAKE3 哈希**：使用 BLAKE3 算法确保哈希稳定性和性能
2. **LRU 淘汰**：按访问时间淘汰，淘汰到 90% 容量
3. **原子操作**：写入新缓存条目时使用临时文件+重命名，避免读取到部分写入的数据
4. **线程安全**：LocalDiskCache 实例仅在主线程使用，无需加锁
5. **错误处理**：缓存读写失败不影响编译流程（降级为无缓存编译）

---

## 验收标准

### 单元测试

```cpp
// tests/unit/IR/LocalDiskCacheTest.cpp

#include "gtest/gtest.h"
#include "blocktype/IR/IRCache.h"
#include <filesystem>

using namespace blocktype;

TEST(LocalDiskCache, StoreAndLookup) {
  cache::CacheKey Key;
  Key.CombinedHash = 12345;
  
  cache::CacheEntry Entry;
  Entry.Key = Key;
  Entry.IRData = {1, 2, 3, 4, 5};
  Entry.ObjectData = {6, 7, 8, 9, 10};
  
  cache::LocalDiskCache LDC("/tmp/test_cache", 1024*1024);
  
  EXPECT_TRUE(LDC.store(Key, Entry));
  
  auto Found = LDC.lookup(Key);
  EXPECT_TRUE(Found.has_value());
  EXPECT_EQ(Found->IRData.size(), 5u);
  EXPECT_EQ(Found->ObjectData.size(), 5u);
}

TEST(LocalDiskCache, Miss) {
  cache::CacheKey Key;
  Key.CombinedHash = 99999;
  
  cache::LocalDiskCache LDC("/tmp/test_cache2", 1024*1024);
  
  auto Found = LDC.lookup(Key);
  EXPECT_FALSE(Found.has_value());
}

TEST(LocalDiskCache, Stats) {
  cache::LocalDiskCache LDC("/tmp/test_cache3", 1024*1024);
  
  cache::CacheKey Key1, Key2;
  Key1.CombinedHash = 11111;
  Key2.CombinedHash = 22222;
  
  cache::CacheEntry Entry;
  Entry.IRData = {1, 2, 3};
  
  LDC.store(Key1, Entry);
  LDC.lookup(Key1);  // Hit
  LDC.lookup(Key2);  // Miss
  
  auto Stats = LDC.getStats();
  EXPECT_EQ(Stats.Hits, 1u);
  EXPECT_EQ(Stats.Misses, 1u);
}

TEST(LocalDiskCache, Eviction) {
  // 设置很小的缓存大小，触发淘汰
  cache::LocalDiskCache LDC("/tmp/test_cache4", 100);
  
  for (int i = 0; i < 100; ++i) {
    cache::CacheKey Key;
    Key.CombinedHash = i;
    
    cache::CacheEntry Entry;
    Entry.IRData = std::vector<uint8_t>(1000, 0);  // 1KB
    
    LDC.store(Key, Entry);
  }
  
  auto Stats = LDC.getStats();
  EXPECT_GT(Stats.Evictions, 0u);  // 应该有淘汰
}

TEST(CacheKey, Compute) {
  cache::CacheOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";
  
  auto Key = cache::CacheKey::compute("int main(){}", Opts);
  
  EXPECT_NE(Key.SourceHash, 0u);
  EXPECT_NE(Key.OptionsHash, 0u);
  EXPECT_NE(Key.CombinedHash, 0u);
}

TEST(CacheKey, ToHex) {
  cache::CacheKey Key;
  Key.CombinedHash = 0xabcdef1234567890;
  
  std::string Hex = Key.toHex();
  EXPECT_EQ(Hex.length(), 16u);  // 64 位 = 16 hex 字符
}
```

### 集成测试

```bash
# V1: 启用缓存编译
# blocktype --fcache-dir=/tmp/cache --fcache-size=1G test.cpp -o test
# 第二次编译应该命中缓存

# V2: 缓存统计
# blocktype --fcache-dir=/tmp/cache --fcache-stats test.cpp -o test
# 输出：Hits: X, Misses: Y, Evictions: Z, TotalSize: W

# V3: 禁用缓存
# blocktype --fcache-no test.cpp -o test
# 不使用缓存

# V4: 缓存命中验证
# 第一次编译
# blocktype --fcache-dir=/tmp/cache test.cpp -o test
# rm test
# 第二次编译（应该命中缓存）
# time blocktype --fcache-dir=/tmp/cache test.cpp -o test
# 耗时应该明显减少
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F8 — LocalDiskCache 基础实现" && git push origin HEAD
```
