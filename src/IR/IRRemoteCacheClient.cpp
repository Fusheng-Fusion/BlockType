#include "blocktype/IR/IRRemoteCacheClient.h"

namespace blocktype {
namespace cache {

IRRemoteCacheClient::IRRemoteCacheClient(ir::StringRef URL, ir::StringRef Bucket)
    : Endpoint_(URL.str()), Bucket_(Bucket.str()) {}

std::optional<CacheEntry> IRRemoteCacheClient::lookup(const CacheKey& /*Key*/) {
  // 桩实现：远程缓存暂不可用，始终返回 nullopt。
  ++Stats_.Misses;
  return std::nullopt;
}

bool IRRemoteCacheClient::store(const CacheKey& /*Key*/,
                                 const CacheEntry& /*Entry*/) {
  // 桩实现：远程缓存暂不可用，始终返回 false。
  return false;
}

bool IRRemoteCacheClient::verifySignature(const CacheEntry& /*Entry*/) const {
  // 桩实现：签名验证暂不实现，始终返回 true。
  return true;
}

} // namespace cache
} // namespace blocktype
