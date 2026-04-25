#ifndef BLOCKTYPE_IR_DIALECTLOWERINGPASS_H
#define BLOCKTYPE_IR_DIALECTLOWERINGPASS_H

#include <functional>

#include "blocktype/IR/IRDialect.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRPass.h"

namespace blocktype {
namespace ir {

/// DialectLoweringPass — 将后端不支持的 Dialect 指令降级为 Core 指令。
class DialectLoweringPass : public Pass {
public:
  /// 构造时传入后端的 Dialect 能力
  explicit DialectLoweringPass(
      dialect::DialectCapability BackendCap = dialect::BackendDialectCaps::LLVM())
    : BackendCap_(BackendCap) {}

  StringRef getName() const override { return "dialect-lowering"; }
  bool run(IRModule& M) override;

private:
  dialect::DialectCapability BackendCap_;

  /// 降级规则
  struct LoweringRule {
    dialect::DialectID SourceDialect;
    Opcode SourceOpcode;
    dialect::DialectID TargetDialect;
    std::function<bool(IRInstruction&)> Lower;
  };

  /// 获取降级规则表
  static const SmallVector<LoweringRule, 16>& getLoweringRules();

  /// 判断指令是否需要降级
  bool needsLowering(const IRInstruction& I) const;

  /// 对单条指令执行降级，返回是否成功
  bool lowerInstruction(IRInstruction& I) const;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_DIALECTLOWERINGPASS_H
