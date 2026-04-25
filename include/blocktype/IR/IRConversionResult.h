#ifndef BLOCKTYPE_IR_IRCONVERSIONRESULT_H
#define BLOCKTYPE_IR_IRCONVERSIONRESULT_H

#include <memory>
#include <string>

#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;

// ============================================================
// IRConversionResult — IR 转换结果
// ============================================================

/// 封装前端到 IR 转换的结果。
/// 有效转换持有 IRModule；无效转换记录错误数量。
class IRConversionResult {
  std::unique_ptr<IRModule> Module_;
  bool Invalid_ = false;
  unsigned NumErrors_ = 0;

public:
  IRConversionResult() = default;

  explicit IRConversionResult(std::unique_ptr<IRModule> M)
    : Module_(std::move(M)) {}

  static IRConversionResult getInvalid(unsigned Errors = 1) {
    IRConversionResult R;
    R.Invalid_ = true;
    R.NumErrors_ = Errors;
    return R;
  }

  bool isInvalid() const { return Invalid_; }
  bool isUsable() const { return Module_ != nullptr && !Invalid_; }
  std::unique_ptr<IRModule> takeModule() { return std::move(Module_); }
  unsigned getNumErrors() const { return NumErrors_; }

  /// Record a conversion error. Increments error count and marks as invalid.
  void addError() {
    ++NumErrors_;
    Invalid_ = true;
  }

  /// Get the module pointer (does not transfer ownership).
  IRModule* getModule() const { return Module_.get(); }
};

// ============================================================
// IRVerificationResult — IR 验证结果
// ============================================================

/// 封装 IR 验证结果。支持收集多条违规信息，一次性报告。
class IRVerificationResult {
  bool IsValid_;
  SmallVector<std::string, 8> Violations_;

public:
  explicit IRVerificationResult(bool Valid) : IsValid_(Valid) {}

  void addViolation(const std::string& Msg) {
    Violations_.push_back(Msg);
    IsValid_ = false;
  }

  bool isValid() const { return IsValid_; }
  const SmallVector<std::string, 8>& getViolations() const { return Violations_; }
  size_t getNumViolations() const { return Violations_.size(); }
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCONVERSIONRESULT_H
