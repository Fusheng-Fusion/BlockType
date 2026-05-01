#ifndef BLOCKTYPE_IR_IRREMOTECACHECLIENT_H
#define BLOCKTYPE_IR_IRREMOTECACHECLIENT_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRCache.h"

namespace blocktype {
namespace cache {

// ============================================================================
// IRRemoteCacheClient — 远程缓存客户端（桩实现）
// ============================================================================

class IRRemoteCacheClient : public CacheStorage {
  std::string Endpoint_;
  std::string Bucket_;
  uint64_t TimeoutMs_ = 5000;
  unsigned MaxRetries_ = 3;
  Stats Stats_;

public:
  explicit IRRemoteCacheClient(ir::StringRef URL, ir::StringRef Bucket = "default");

  // === CacheStorage 接口 ===

  std::optional<CacheEntry> lookup(const CacheKey& Key) override;
  bool store(const CacheKey& Key, const CacheEntry& Entry) override;
  Stats getStats() const override { return Stats_; }

  // === 增强功能 ===

  /// 验证缓存条目的签名。桩实现始终返回 true。
  bool verifySignature(const CacheEntry& Entry) const;

  void setTimeout(uint64_t Ms) { TimeoutMs_ = Ms; }
  void setMaxRetries(unsigned N) { MaxRetries_ = N; }
  uint64_t getTimeout() const { return TimeoutMs_; }
  unsigned getMaxRetries() const { return MaxRetries_; }
  ir::StringRef getEndpoint() const { return Endpoint_; }
  ir::StringRef getBucket() const { return Bucket_; }
};

} // namespace cache
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRREMOTECACHECLIENT_H
