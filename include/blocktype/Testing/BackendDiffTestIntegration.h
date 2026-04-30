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
