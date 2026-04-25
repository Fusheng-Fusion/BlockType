#include <gtest/gtest.h>
#include <cstring>
#include "blocktype/IR/IRErrorCode.h"

using namespace blocktype;
using namespace blocktype::ir;

// ============================================================
// V1: 错误码转字符串 — 类型错误段
// ============================================================
TEST(IRErrorCodeTest, TypeErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeMappingFailed),
               "TypeMappingFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeIncomplete),
               "TypeIncomplete");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeMismatch),
               "TypeMismatch");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeCircularRef),
               "TypeCircularRef");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeInvalidBitWidth),
               "TypeInvalidBitWidth");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TypeStructBodyConflict),
               "TypeStructBodyConflict");
}

// ============================================================
// V2: 错误码转字符串 — 指令错误段
// ============================================================
TEST(IRErrorCodeTest, InstructionErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidOpcode),
               "InvalidOpcode");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidOperand),
               "InvalidOperand");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::MissingTerminator),
               "MissingTerminator");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::MultipleTerminators),
               "MultipleTerminators");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidPHINode),
               "InvalidPHINode");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidGEPIndex),
               "InvalidGEPIndex");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidCallSignature),
               "InvalidCallSignature");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidInvokeTarget),
               "InvalidInvokeTarget");
}

// ============================================================
// V3: 错误码转字符串 — 验证错误段
// ============================================================
TEST(IRErrorCodeTest, VerificationErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::SSAViolation),
               "SSAViolation");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::DefUseChainBroken),
               "DefUseChainBroken");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::UseDefChainBroken),
               "UseDefChainBroken");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::GlobalRefNotFound),
               "GlobalRefNotFound");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::FunctionRefNotFound),
               "FunctionRefNotFound");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::BlockRefNotFound),
               "BlockRefNotFound");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidEntryBlock),
               "InvalidEntryBlock");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::EmptyFunction),
               "EmptyFunction");
}

// ============================================================
// V4: 错误码转字符串 — 序列化错误段
// ============================================================
TEST(IRErrorCodeTest, SerializationErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidFormat),
               "InvalidFormat");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::VersionMismatch),
               "VersionMismatch");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::ChecksumFailed),
               "ChecksumFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::SignatureFailed),
               "SignatureFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::InvalidMagicNumber),
               "InvalidMagicNumber");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::TruncatedData),
               "TruncatedData");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::StringTableError),
               "StringTableError");
}

// ============================================================
// V5: 错误码转字符串 — 后端错误段
// ============================================================
TEST(IRErrorCodeTest, BackendErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::BackendUnsupportedFeature),
               "BackendUnsupportedFeature");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::BackendLoweringFailed),
               "BackendLoweringFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::BackendCodegenFailed),
               "BackendCodegenFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::BackendOptimizationFailed),
               "BackendOptimizationFailed");
}

// ============================================================
// V6: 错误码转字符串 — FFI 错误段
// ============================================================
TEST(IRErrorCodeTest, FFIErrorsToString) {
  EXPECT_STREQ(errorCodeToString(IRErrorCode::FFITypeMappingFailed),
               "FFITypeMappingFailed");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::FFICallingConvMismatch),
               "FFICallingConvMismatch");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::FFIHeaderNotFound),
               "FFIHeaderNotFound");
  EXPECT_STREQ(errorCodeToString(IRErrorCode::FFIBindingGenerationFailed),
               "FFIBindingGenerationFailed");
}

// ============================================================
// V7: 错误码分段验证
// ============================================================
TEST(IRErrorCodeTest, ErrorCodeSegments) {
  // 类型错误段 → 10
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::TypeMappingFailed) / 100, 10u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::TypeStructBodyConflict) / 100, 10u);

  // 指令错误段 → 11
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::InvalidOpcode) / 100, 11u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::InvalidInvokeTarget) / 100, 11u);

  // 验证错误段 → 12
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::SSAViolation) / 100, 12u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::EmptyFunction) / 100, 12u);

  // 序列化错误段 → 13
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::InvalidFormat) / 100, 13u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::StringTableError) / 100, 13u);

  // 后端错误段 → 14
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::BackendUnsupportedFeature) / 100, 14u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::BackendOptimizationFailed) / 100, 14u);

  // FFI 错误段 → 15
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::FFITypeMappingFailed) / 100, 15u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::FFIBindingGenerationFailed) / 100, 15u);
}

// ============================================================
// V8: getErrorCodeSegment 辅助函数
// ============================================================
TEST(IRErrorCodeTest, GetErrorCodeSegment) {
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::TypeMappingFailed), 10u);
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::InvalidOpcode), 11u);
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::SSAViolation), 12u);
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::InvalidFormat), 13u);
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::BackendLoweringFailed), 14u);
  EXPECT_EQ(getErrorCodeSegment(IRErrorCode::FFIHeaderNotFound), 15u);
}

// ============================================================
// V9: 底层类型和大小
// ============================================================
TEST(IRErrorCodeTest, UnderlyingType) {
  EXPECT_EQ(sizeof(IRErrorCode), sizeof(uint32_t));
  // 验证具体数值
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::TypeMappingFailed), 1001u);
  EXPECT_EQ(static_cast<uint32_t>(IRErrorCode::FFIBindingGenerationFailed), 1504u);
}
