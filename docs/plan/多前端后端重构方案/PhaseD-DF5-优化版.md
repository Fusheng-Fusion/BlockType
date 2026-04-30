# Phase D-F5 — BackendDiffTestIntegration 后端差分测试（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **`DifferentialTester` 类不存在**：
   - Spec 中 `BackendDiffTestIntegration::testBackendEquivalence()` 返回 `DifferentialTester::DiffResult`。
   - `DifferentialTester` 类在整个代码库中**完全不存在**。
   - **修复**：此任务需**自行定义** `DifferentialTester` 类及其内部类型（`Config::Granularity`、`DiffResult`），或者将 `BackendDiffTestIntegration` 设计为不依赖外部 `DifferentialTester` 的独立接口。

2. **`IRFuzzer` 类不存在**：
   - Spec 中 `BackendFuzzIntegration::fuzzBackend()` 返回 `IRFuzzer::FuzzResult`。
   - `IRFuzzer` 类在整个代码库中**完全不存在**。
   - **修复**：此任务需**自行定义** `IRFuzzer` 类及其 `FuzzResult` 类型，或者将 `BackendFuzzIntegration` 设计为不依赖外部 `IRFuzzer` 的独立接口。

3. **Spec 将两个不相关的功能合并在同一任务中**：
   - `BackendDiffTestIntegration`（差分测试）和 `BackendFuzzIntegration`（模糊测试）是两个独立的功能。
   - **修复**：此任务聚焦于 `BackendDiffTestIntegration`（差分测试），将 `BackendFuzzIntegration` 延后到后续 Phase。

### P2 — 建议修复
1. **`BackendDiffConfig::Level` 的 `DifferentialTester::Config::Granularity` 类型未定义**：
   - 由于 `DifferentialTester` 不存在，这个类型也不存在。
   - **修复**：定义 `DiffGranularity` 枚举：`InstructionLevel`, `FunctionLevel`, `ModuleLevel`。

2. **`testBackendEquivalence` 的实现依赖多个后端**：
   - 验收标准只创建了一个后端 `{"llvm"}`，但差分测试的意义在于对比**不同**后端的输出。
   - **修复**：初期实现对比同一后端在不同优化级别下的输出，或对比旧管线 vs 新管线输出。

3. **编译选项与现有命令行体系冲突**：
   - `--ftest-differential` 等选项使用 `-f` 前缀，与现有 `-fcontract-mode` 等风格一致。
   - 但这些选项的功能是测试工具，不应出现在生产编译器中。
   - **修复**：仅在测试可执行文件中提供这些选项，或使用 `--test-differential` 前缀。

### P3 — 建议改进
1. 差分测试应有超时机制，防止某个后端挂起。
2. 应支持输出详细的 diff 报告（哪些函数/指令不同）。

## 依赖分析

- **前置依赖**：C.9（集成测试）— 但 C.9 的差分测试仅对比新旧管线，此任务是**后端级别**的差分
- **硬依赖**：`BackendRegistry`（已实现），`IRModule`（已实现）
- **缺失依赖**：`DifferentialTester`、`IRFuzzer`（需在此任务中定义）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Testing/BackendDiffTestIntegration.h` |
| 新增 | `tests/diff/BackendDiffTest.cpp` |

## 当前代码状态

- `BackendRegistry` 已实现，支持创建后端实例。
- `IRModule` 已实现。
- 无 `DifferentialTester`、`IRFuzzer` 类。
- 无 `blocktype/Testing/` 目录。
- 测试框架使用 GTest。

## 实现步骤

1. **创建 `include/blocktype/Testing/` 目录**。

2. **定义差分测试核心类型**：
   ```cpp
   namespace blocktype::testing {

   /// 差分粒度
   enum class DiffGranularity {
     InstructionLevel,  // 逐指令对比
     FunctionLevel,     // 逐函数对比
     ModuleLevel,       // 模块级对比
   };

   /// 差分结果
   struct DiffResult {
     bool IsEquivalent = true;
     std::string DiffDescription;
     unsigned NumDifferences = 0;
   };

   /// 差分测试配置
   struct BackendDiffConfig {
     std::vector<std::string> BackendNames;
     DiffGranularity Level = DiffGranularity::FunctionLevel;
   };

   /// 后端差分测试集成
   class BackendDiffTestIntegration {
   public:
     /// 对比同一 IRModule 经不同后端编译的结果
     static DiffResult testBackendEquivalence(
       const ir::IRModule& M,
       const BackendDiffConfig& Cfg,
       backend::BackendRegistry& Registry);
   };

   } // namespace blocktype::testing
   ```

3. **创建 `tests/diff/BackendDiffTest.cpp`**：
   - 实现 `testBackendEquivalence()` 的基本版本
   - 使用 `BackendRegistry` 创建后端实例
   - 编译同一 `IRModule`，对比输出

4. **在 `CMakeLists.txt` 中注册测试**。

## 接口签名（最终版）

```cpp
#pragma once
#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/IR/IRModule.h"
#include <string>
#include <vector>

namespace blocktype::testing {

/// 差分粒度
enum class DiffGranularity {
  InstructionLevel,
  FunctionLevel,
  ModuleLevel,
};

/// 差分结果
struct DiffResult {
  bool IsEquivalent = true;
  std::string DiffDescription;
  unsigned NumDifferences = 0;
};

/// 后端差分测试配置
struct BackendDiffConfig {
  /// 参与对比的后端名称列表（至少 2 个才有意义）
  std::vector<std::string> BackendNames;
  /// 对比粒度
  DiffGranularity Level = DiffGranularity::FunctionLevel;
};

/// 后端差分测试集成
class BackendDiffTestIntegration {
public:
  /// 对比同一 IRModule 经不同后端编译的结果
  /// \param M        待编译的 IR 模块
  /// \param Cfg      差分配置（后端列表、粒度）
  /// \param Registry 后端注册表
  /// \returns 差分结果
  static DiffResult testBackendEquivalence(
    const ir::IRModule& M,
    const BackendDiffConfig& Cfg,
    backend::BackendRegistry& Registry);
};

/// 后端模糊测试集成（远期 — 仅声明接口）
class BackendFuzzIntegration {
public:
  struct BackendFuzzConfig {
    std::string BackendName;
    uint64_t MaxIterations = 1000;
    uint64_t Seed = 42;
  };

  struct FuzzResult {
    bool FoundIssue = false;
    std::string IssueDescription;
    uint64_t IterationsRun = 0;
  };

  static FuzzResult fuzzBackend(
    const BackendFuzzConfig& Cfg,
    backend::BackendRegistry& Registry);
};

} // namespace blocktype::testing
```

## 测试计划

```cpp
// T1: BackendDiffConfig 创建
testing::BackendDiffConfig Cfg;
Cfg.BackendNames = {"llvm"};
EXPECT_EQ(Cfg.BackendNames.size(), 1u);
EXPECT_EQ(Cfg.Level, testing::DiffGranularity::FunctionLevel);

// T2: DiffResult 默认值
testing::DiffResult Result;
EXPECT_TRUE(Result.IsEquivalent);
EXPECT_EQ(Result.NumDifferences, 0u);

// T3: BackendDiffTestIntegration 基本调用
// 构造一个简单 IRModule
// 调用 testBackendEquivalence()
// 验证返回值类型正确

// T4: 单后端差分测试（与自身对比，应等价）
testing::BackendDiffConfig Cfg;
Cfg.BackendNames = {"llvm"};
auto Result = testing::BackendDiffTestIntegration::testBackendEquivalence(
  Module, Cfg, backend::BackendRegistry::instance());
EXPECT_TRUE(Result.IsEquivalent);
```

## 风险点

1. **当前只有一个后端（LLVM）**：差分测试需要至少两个后端才有意义。初期只能做"自对比"（同一后端两次编译的结果对比）。
2. **二进制输出的不确定性**：编译器输出可能包含时间戳、UUID 等不确定内容，导致等价对比失败。需过滤这些差异。
3. **`BackendFuzzIntegration` 的依赖链过长**：模糊测试需要 IR 生成器、执行环境等基础设施。建议推迟到远期 Phase。
4. **`Testing/` 目录是新增的**：需确保 CMake 构建系统正确包含新目录。
