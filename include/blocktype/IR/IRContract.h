#ifndef BLOCKTYPE_IR_IRCONTRACT_H
#define BLOCKTYPE_IR_IRCONTRACT_H

#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRVerificationResult;

namespace contract {

/// 验证模块级契约：名称非空。
bool verifyIRModuleContract(const IRModule& M);

/// 验证所有类型完整性：无 OpaqueType 出现在函数签名或全局变量中。
bool verifyTypeCompleteness(const IRModule& M);

/// 验证函数非空：所有定义函数至少有一个 BasicBlock。
bool verifyFunctionNonEmpty(const IRModule& M);

/// 验证终结指令契约：每个 BasicBlock 都有终结指令。
bool verifyTerminatorContract(const IRModule& M);

/// 验证类型一致性：指令操作数类型与指令语义匹配。
bool verifyTypeConsistency(const IRModule& M);

/// 验证 TargetTriple 格式：如果设置了 TargetTriple，应包含至少一个 '-'。
bool verifyTargetTripleValid(const IRModule& M);

/// 执行所有契约验证，返回聚合结果。
IRVerificationResult verifyAllContracts(const IRModule& M);

} // namespace contract
} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCONTRACT_H
