#ifndef BLOCKTYPE_IR_IRERRORCODE_H
#define BLOCKTYPE_IR_IRERRORCODE_H

#include <cstdint>

namespace blocktype {
namespace ir {

// ============================================================
// IRErrorCode — IR 层错误码枚举
// ============================================================

/// 错误码分段规则：
///   1000-1099 类型错误 (Type)
///   1100-1199 指令错误 (Instruction)
///   1200-1299 验证错误 (Verification)
///   1300-1399 序列化错误 (Serialization)
///   1400-1499 后端错误 (Backend)
///   1500-1599 FFI 错误 (FFI)
enum class IRErrorCode : uint32_t {
  // 类型错误 (1000-1099)
  TypeMappingFailed      = 1001,
  TypeIncomplete         = 1002,
  TypeMismatch           = 1003,
  TypeCircularRef        = 1004,
  TypeInvalidBitWidth    = 1005,
  TypeStructBodyConflict = 1006,

  // 指令错误 (1100-1199)
  InvalidOpcode          = 1101,
  InvalidOperand         = 1102,
  MissingTerminator      = 1103,
  MultipleTerminators    = 1104,
  InvalidPHINode         = 1105,
  InvalidGEPIndex        = 1106,
  InvalidCallSignature   = 1107,
  InvalidInvokeTarget    = 1108,

  // 验证错误 (1200-1299)
  SSAViolation           = 1201,
  DefUseChainBroken      = 1202,
  UseDefChainBroken      = 1203,
  GlobalRefNotFound      = 1204,
  FunctionRefNotFound    = 1205,
  BlockRefNotFound       = 1206,
  InvalidEntryBlock      = 1207,
  EmptyFunction          = 1208,

  // 序列化错误 (1300-1399)
  InvalidFormat          = 1301,
  VersionMismatch        = 1302,
  ChecksumFailed         = 1303,
  SignatureFailed        = 1304,
  InvalidMagicNumber     = 1305,
  TruncatedData          = 1306,
  StringTableError       = 1307,

  // 后端错误 (1400-1499)
  BackendUnsupportedFeature = 1401,
  BackendLoweringFailed  = 1402,
  BackendCodegenFailed   = 1403,
  BackendOptimizationFailed = 1404,

  // FFI错误 (1500-1599)
  FFITypeMappingFailed   = 1501,
  FFICallingConvMismatch = 1502,
  FFIHeaderNotFound      = 1503,
  FFIBindingGenerationFailed = 1504,
};

/// 将 IRErrorCode 转换为枚举值名称字符串。
/// 返回值永远非空；未匹配时返回 "UnknownErrorCode"。
const char* errorCodeToString(IRErrorCode Code);

/// 获取错误码所属的分段编号。
/// 例如 TypeMappingFailed(1001) → 10，InvalidOpcode(1101) → 11。
inline constexpr uint32_t getErrorCodeSegment(IRErrorCode Code) {
  return static_cast<uint32_t>(Code) / 100;
}

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRERRORCODE_H
