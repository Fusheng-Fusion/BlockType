#pragma once
#include "blocktype/Backend/InstructionSelector.h"
#include "blocktype/IR/IRType.h"  // ir::dialect::DialectID

namespace blocktype::backend {

/// 目标降低抽象基类
/// 负责将非 Core Dialect 的 IR 指令降低到目标指令
class TargetLowering {
public:
  virtual ~TargetLowering() = default;

  /// 将一条 IR 指令降低到目标指令序列
  virtual bool lower(const ir::IRInstruction& I,
                     TargetInstructionList& Output) = 0;

  /// 检查是否支持指定 Dialect
  virtual bool supportsDialect(ir::dialect::DialectID D) const = 0;
};

} // namespace blocktype::backend
