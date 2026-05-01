#ifndef BLOCKTYPE_IR_IRPASSES_H
#define BLOCKTYPE_IR_IRPASSES_H

#include <memory>

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRPass.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRConstantInt;

// ============================================================================
// PassManager — 管理并顺序执行 IR Pass
// ============================================================================

class PassManager {
  SmallVector<std::unique_ptr<Pass>, 8> Passes_;

public:
  PassManager() = default;

  /// 添加一个 Pass 到管线末尾。
  template <typename PassT, typename... Args>
  void addPass(Args&&... args) {
    Passes_.push_back(std::make_unique<PassT>(std::forward<Args>(args)...));
  }

  /// 顺序执行所有 Pass。返回 true 如果任意 Pass 返回 true（即修改了 IR）。
  bool runAll(IRModule& M);

  /// 通过 unique_ptr 直接添加一个 Pass 到管线末尾（用于插件动态注册场景）。
  void addPassPtr(std::unique_ptr<Pass> P) { Passes_.push_back(std::move(P)); }

  /// 返回管线中的 Pass 数量。
  size_t size() const { return Passes_.size(); }
};

// ============================================================================
// DeadFunctionEliminationPass — 移除无用的内部声明函数
// ============================================================================

class DeadFunctionEliminationPass : public Pass {
public:
  StringRef getName() const override { return "dead-func-elim"; }

  /// 移除所有 isDeclaration() && Linkage == Internal 的函数。
  /// 返回 true 如果有函数被移除。
  bool run(IRModule& M) override;
};

// ============================================================================
// ConstantFoldingPass — 常量折叠
// ============================================================================

class ConstantFoldingPass : public Pass {
public:
  StringRef getName() const override { return "constant-fold"; }

  /// 对所有整数算术指令做常量折叠。
  /// 返回 true 如果有指令被折叠。
  bool run(IRModule& M) override;

private:
  /// 尝试折叠一条指令。返回折叠后的常量，或 std::nullopt。
  std::optional<IRConstantInt*> tryFold(class IRInstruction& I);
};

// ============================================================================
// TypeCanonicalizationPass — 类型规范化
// ============================================================================

class TypeCanonicalizationPass : public Pass {
public:
  StringRef getName() const override { return "type-canon"; }

  /// 轻量级类型规范化检查。当前阶段为占位实现。
  bool run(IRModule& M) override;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRPASSES_H
