#ifndef BLOCKTYPE_IR_IRFUZZER_H
#define BLOCKTYPE_IR_IRFUZZER_H

#include <cstdint>
#include <functional>
#include <vector>

#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

// ============================================================================
// OracleFn — 判断输入是否"有趣"的谓词函数
// ============================================================================

using OracleFn = std::function<bool(ir::ArrayRef<uint8_t>)>;

// ============================================================================
// IRFuzzer — IR 模糊测试基础框架
// ============================================================================

class IRFuzzer {
  unsigned MaxInputSize_ = 4096;
  unsigned Seed_ = 42;

public:
  /// Delta Debugging 最小化：从触发输入中移除不相关字节，
  /// 返回仍满足 Oracle 的最小子集。
  static std::vector<uint8_t> deltaDebug(ir::ArrayRef<uint8_t> TriggeringInput,
                                          OracleFn Oracle);

  /// 基于内部 Seed_ 生成确定性伪随机输入。
  std::vector<uint8_t> generateRandomInput(size_t Size);

  /// 基于 Seed 对输入做确定性字节级变异。
  static std::vector<uint8_t> mutate(ir::ArrayRef<uint8_t> Input, unsigned Seed);

  void setMaxInputSize(unsigned N) { MaxInputSize_ = N; }
  void setSeed(unsigned S) { Seed_ = S; }
  unsigned getMaxInputSize() const { return MaxInputSize_; }
  unsigned getSeed() const { return Seed_; }
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRFUZZER_H
