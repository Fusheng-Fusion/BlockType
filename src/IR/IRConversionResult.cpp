#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRConversionResult.h"

// IRConversionResult 和 IRVerificationResult 的实现全部在头文件中（inline）。
// 此 .cpp 文件存在是为了：
// 1. 确保头文件的自包含性（编译验证）——通过先 include IRModule.h 确保在
//    IRConversionResult.h 被编译时 IRModule 是完整类型
// 2. 为后续扩展预留（如添加 toString()、日志等方法）

namespace blocktype {
namespace ir {

// 预留后续扩展：
// - IRConversionResult::toString() — 生成人类可读的结果描述
// - IRVerificationResult::toJSON() — 生成结构化的验证报告

} // namespace ir
} // namespace blocktype
