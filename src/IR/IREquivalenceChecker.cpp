#include "blocktype/IR/IREquivalenceChecker.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRType.h"

namespace blocktype {
namespace ir {

IREquivalenceChecker::EquivalenceResult
IREquivalenceChecker::check(const IRModule& A, const IRModule& B) {
  EquivalenceResult Result;
  // Stub: 比较函数数量
  if (A.getNumFunctions() != B.getNumFunctions()) {
    Result.IsEquivalent = false;
    Result.Differences.push_back("Different number of functions");
    return Result;
  }
  // Stub: 简单名称对比
  auto& FA = A.getFunctions();
  auto& FB = B.getFunctions();
  auto ItA = FA.begin();
  auto ItB = FB.begin();
  for (; ItA != FA.end() && ItB != FB.end(); ++ItA, ++ItB) {
    if ((*ItA)->getName() != (*ItB)->getName()) {
      Result.Differences.push_back(
        "Function name mismatch: " + (*ItA)->getName().str() +
        " vs " + (*ItB)->getName().str());
    }
  }
  Result.IsEquivalent = Result.Differences.empty();
  return Result;
}

bool IREquivalenceChecker::isStructurallyEquivalent(
    const IRFunction& A, const IRFunction& B) {
  // Stub: 比较名称和参数数量
  if (A.getName() != B.getName()) return false;
  if (A.getNumArgs() != B.getNumArgs()) return false;
  if (A.getNumBasicBlocks() != B.getNumBasicBlocks()) return false;
  return true;
}

bool IREquivalenceChecker::isTypeEquivalent(const IRType* A, const IRType* B) {
  if (!A && !B) return true;
  if (!A || !B) return false;
  return A->equals(B);
}

} // namespace ir
} // namespace blocktype
