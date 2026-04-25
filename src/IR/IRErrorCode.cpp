#include "blocktype/IR/IRErrorCode.h"

namespace blocktype {
namespace ir {

const char* errorCodeToString(IRErrorCode Code) {
  switch (Code) {
    // 类型错误 (1000-1099)
    case IRErrorCode::TypeMappingFailed:      return "TypeMappingFailed";
    case IRErrorCode::TypeIncomplete:         return "TypeIncomplete";
    case IRErrorCode::TypeMismatch:           return "TypeMismatch";
    case IRErrorCode::TypeCircularRef:        return "TypeCircularRef";
    case IRErrorCode::TypeInvalidBitWidth:    return "TypeInvalidBitWidth";
    case IRErrorCode::TypeStructBodyConflict: return "TypeStructBodyConflict";

    // 指令错误 (1100-1199)
    case IRErrorCode::InvalidOpcode:        return "InvalidOpcode";
    case IRErrorCode::InvalidOperand:       return "InvalidOperand";
    case IRErrorCode::MissingTerminator:    return "MissingTerminator";
    case IRErrorCode::MultipleTerminators:  return "MultipleTerminators";
    case IRErrorCode::InvalidPHINode:       return "InvalidPHINode";
    case IRErrorCode::InvalidGEPIndex:      return "InvalidGEPIndex";
    case IRErrorCode::InvalidCallSignature: return "InvalidCallSignature";
    case IRErrorCode::InvalidInvokeTarget:  return "InvalidInvokeTarget";

    // 验证错误 (1200-1299)
    case IRErrorCode::SSAViolation:        return "SSAViolation";
    case IRErrorCode::DefUseChainBroken:   return "DefUseChainBroken";
    case IRErrorCode::UseDefChainBroken:   return "UseDefChainBroken";
    case IRErrorCode::GlobalRefNotFound:   return "GlobalRefNotFound";
    case IRErrorCode::FunctionRefNotFound: return "FunctionRefNotFound";
    case IRErrorCode::BlockRefNotFound:    return "BlockRefNotFound";
    case IRErrorCode::InvalidEntryBlock:   return "InvalidEntryBlock";
    case IRErrorCode::EmptyFunction:       return "EmptyFunction";

    // 序列化错误 (1300-1399)
    case IRErrorCode::InvalidFormat:      return "InvalidFormat";
    case IRErrorCode::VersionMismatch:    return "VersionMismatch";
    case IRErrorCode::ChecksumFailed:     return "ChecksumFailed";
    case IRErrorCode::SignatureFailed:    return "SignatureFailed";
    case IRErrorCode::InvalidMagicNumber: return "InvalidMagicNumber";
    case IRErrorCode::TruncatedData:      return "TruncatedData";
    case IRErrorCode::StringTableError:   return "StringTableError";

    // 后端错误 (1400-1499)
    case IRErrorCode::BackendUnsupportedFeature: return "BackendUnsupportedFeature";
    case IRErrorCode::BackendLoweringFailed:     return "BackendLoweringFailed";
    case IRErrorCode::BackendCodegenFailed:      return "BackendCodegenFailed";
    case IRErrorCode::BackendOptimizationFailed: return "BackendOptimizationFailed";

    // FFI错误 (1500-1599)
    case IRErrorCode::FFITypeMappingFailed:       return "FFITypeMappingFailed";
    case IRErrorCode::FFICallingConvMismatch:     return "FFICallingConvMismatch";
    case IRErrorCode::FFIHeaderNotFound:          return "FFIHeaderNotFound";
    case IRErrorCode::FFIBindingGenerationFailed: return "FFIBindingGenerationFailed";
  }
  return "UnknownErrorCode";
}

} // namespace ir
} // namespace blocktype
