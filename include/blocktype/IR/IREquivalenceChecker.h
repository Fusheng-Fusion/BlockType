#ifndef BLOCKTYPE_IR_IREQUIVALENCECHECKER_H
#define BLOCKTYPE_IR_IREQUIVALENCECHECKER_H

#include <string>
#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRFunction;
class IRType;

// ============================================================
// IREquivalenceChecker — IR 等价性检查（接口定义）
// ============================================================
//
// 纯接口，实现留待 Phase E (E-F3)。

class IREquivalenceChecker {
public:
  struct EquivalenceResult {
    bool IsEquivalent = false;
    ir::SmallVector<std::string, 8> Differences;
  };

  /// 检查两个 IRModule 是否等价
  static EquivalenceResult check(const IRModule& A, const IRModule& B);

  /// 检查两个 IRFunction 是否结构等价
  static bool isStructurallyEquivalent(const IRFunction& A, const IRFunction& B);

  /// 检查两个 IRType 是否等价
  static bool isTypeEquivalent(const IRType* A, const IRType* B);
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IREQUIVALENCECHECKER_H
