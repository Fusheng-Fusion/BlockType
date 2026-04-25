# Task A-F10 优化版：IRErrorCode 枚举

> 文档版本：v1.0 | 由 planner 产出

---

## 红线 Checklist 确认

| # | 红线 | 状态 | 说明 |
|---|------|------|------|
| 1 | 架构优先 | ✅ 通过 | `IRErrorCode` 为独立枚举，不耦合任何具体前端/后端实现 |
| 2 | 多前端多后端自由组合 | ✅ 通过 | 错误码是通用 IR 层基础设施，所有前端/后端共用 |
| 3 | 渐进式改造 | ✅ 通过 | 纯新增 3 文件 + CMakeLists 追加 2 行，可编译可测试 |
| 4 | 现有功能不退化 | ✅ 通过 | 不修改任何现有文件（IRModule.h、IRDiagnostic.h 等均不改动） |
| 5 | 接口抽象优先 | ✅ 通过 | `errorCodeToString` 为纯函数接口，无具体实现依赖 |
| 6 | IR 中间层解耦 | ✅ 通过 | 错误码定义在 `blocktype::ir` 命名空间，属于 IR 层，前后端不直接交互 |

---

## 一、任务概述

| 项目 | 内容 |
|------|------|
| 任务编号 | A-F10 |
| 任务名称 | IRErrorCode 枚举 |
| 依赖 | A.4（IRModule）— ✅ 已存在（IRFeature 枚举已在 `IRModule.h` 第 18-31 行，无需重复） |
| 对现有代码的影响 | 仅 CMakeLists 追加条目 |

**产出文件**

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRErrorCode.h` |
| 新增 | `src/IR/IRErrorCode.cpp` |
| 新增 | `tests/unit/IR/IRErrorCodeTest.cpp` |
| 修改 | `src/IR/CMakeLists.txt`（+1 行） |
| 修改 | `tests/unit/IR/CMakeLists.txt`（+1 行） |

---

## 二、设计分析

### 2.1 项目适配要点

| 要点 | 适配方案 |
|------|----------|
| `IRFeature` 枚举已存在于 `IRModule.h` | IRErrorCode 独立文件，不修改 `IRModule.h` |
| 命名空间 `blocktype::ir` | 与 `IRFeature`、`BackendCapability` 保持一致 |
| `IRDiagnostic.h` 参考 `const char*` 转函数风格 | `errorCodeToString` 返回 `const char*`，完整 switch-case |
| 枚举底层类型 `uint32_t` | 与 `IRFeature`、`DiagnosticCode` 保持一致 |
| 头文件自包含 | 仅 include `<cstdint>` |

### 2.2 错误码分段设计

| 分段范围 | 类别 | 枚举值数量 |
|----------|------|-----------|
| 1000-1099 | 类型错误 (Type) | 6 个 |
| 1100-1199 | 指令错误 (Instruction) | 8 个 |
| 1200-1299 | 验证错误 (Verification) | 8 个 |
| 1300-1399 | 序列化错误 (Serialization) | 7 个 |
| 1400-1499 | 后端错误 (Backend) | 4 个 |
| 1500-1599 | FFI 错误 (FFI) | 4 个 |

### 2.3 与 IRDiagnostic 的关系

`IRDiagnostic.h` 中的 `DiagnosticCode` 使用 `0xGGNN` 编码（hex），面向诊断系统。
`IRErrorCode` 使用十进制定长分段编码（1001-1504），面向 IR 层错误传播。
二者正交：`DiagnosticCode` 用于诊断报告，`IRErrorCode` 用于 IR 操作结果码。

---

## 三、完整头文件 — `include/blocktype/IR/IRErrorCode.h`

```cpp
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
```

---

## 四、完整实现文件 — `src/IR/IRErrorCode.cpp`

```cpp
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
```

---

## 五、完整测试文件 — `tests/unit/IR/IRErrorCodeTest.cpp`

```cpp
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
```

---

## 六、CMakeLists.txt 修改

### `src/IR/CMakeLists.txt` — 追加：

在源文件列表末尾（`IRConversionResult.cpp` 之后）添加：

```cmake
  IRErrorCode.cpp
```

### `tests/unit/IR/CMakeLists.txt` — 追加：

在源文件列表末尾（`IRConversionResultTest.cpp` 之后）添加：

```cmake
  IRErrorCodeTest.cpp
```

---

## 七、验收标准映射

| 编号 | 验收内容 | 对应测试 |
|------|----------|----------|
| V1 | 类型错误段字符串转换（6 枚举值全覆盖） | `TypeErrorsToString` |
| V2 | 指令错误段字符串转换（8 枚举值全覆盖） | `InstructionErrorsToString` |
| V3 | 验证错误段字符串转换（8 枚举值全覆盖） | `VerificationErrorsToString` |
| V4 | 序列化错误段字符串转换（7 枚举值全覆盖） | `SerializationErrorsToString` |
| V5 | 后端错误段字符串转换（4 枚举值全覆盖） | `BackendErrorsToString` |
| V6 | FFI 错误段字符串转换（4 枚举值全覆盖） | `FFIErrorsToString` |
| V7 | 错误码分段数值验证（每段首尾） | `ErrorCodeSegments` |
| V8 | getErrorCodeSegment 辅助函数 | `GetErrorCodeSegment` |
| V9 | 底层类型 uint32_t + 具体数值 | `UnderlyingType` |

**Spec 原始验收标准覆盖**：

| Spec 验收 | 映射 |
|-----------|------|
| `strcmp(errorCodeToString(TypeMappingFailed), "TypeMappingFailed") == 0` | V1 |
| `static_cast<uint32_t>(TypeMappingFailed) / 100 == 10` | V7 |
| `static_cast<uint32_t>(InvalidOpcode) / 100 == 11` | V7 |
| `static_cast<uint32_t>(SSAViolation) / 100 == 12` | V7 |

---

## 八、sizeof 影响评估

| 类型 | 大小 | 说明 |
|------|------|------|
| `IRErrorCode` | 4 bytes | 等同 `uint32_t` |

---

## 九、风险与注意事项

1. **不修改 `IRModule.h`**：`IRFeature` 枚举已存在，本任务仅新增 `IRErrorCode`，完全独立
2. **switch-case 完整覆盖**：`errorCodeToString` 覆盖全部 37 个枚举值，无 default 分支（编译器 -Wswitch 警告保护）
3. **与 `IRDiagnostic` 的 `DiagnosticCode` 正交**：`DiagnosticCode` 用于诊断报告（hex 编码），`IRErrorCode` 用于 IR 操作结果码（十进制分段），不冲突
4. **`getErrorCodeSegment` 为 inline constexpr**：零开销，编译期可用
5. **红线 Checklist 全部通过**：纯新增、无耦合、不退化

---

## 十、实现步骤

| 步骤 | 操作 |
|------|------|
| 1 | 创建 `include/blocktype/IR/IRErrorCode.h` |
| 2 | 创建 `src/IR/IRErrorCode.cpp` |
| 3 | 修改 `src/IR/CMakeLists.txt`（追加 `IRErrorCode.cpp`） |
| 4 | 修改 `tests/unit/IR/CMakeLists.txt`（追加 `IRErrorCodeTest.cpp`） |
| 5 | 创建 `tests/unit/IR/IRErrorCodeTest.cpp` |
| 6 | 编译 + 测试 + 全量 ctest |
| 7 | Git commit |
