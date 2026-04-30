# AI Coder 可执行任务流 — Phase B 增强任务 B-F5：TelemetryCollector 与 CompilerInstance 集成

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype` 中
5. **头文件路径**：`include/blocktype/Frontend/`，源文件路径：`src/Frontend/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F5 — TelemetryCollector 与 CompilerInstance 集成"
   git push origin HEAD
   ```
   - **不得跳过此步骤**——确保每个 Task 的产出都有远端备份，防止工作丢失
   - 如果 push 失败，先 `git pull --rebase origin HEAD` 再重试

---

## Task B-F5：TelemetryCollector 与 CompilerInstance 集成

### 依赖验证

✅ **A-F3（TelemetryCollector 基础框架）**：已存在于 `include/blocktype/IR/IRTelemetry.h`
✅ **CompilerInstance**：已存在于 `include/blocktype/Frontend/CompilerInstance.h`

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `src/IR/IRTelemetry.cpp` |
| 修改 | `include/blocktype/Frontend/CompilerInstance.h` |
| 修改 | `src/Frontend/CompilerInstance.cpp` |

---

## 必须实现的修改

### 1. 实现 IRTelemetry.cpp

```cpp
// 在 src/IR/IRTelemetry.cpp 中实现

#include "blocktype/IR/IRTelemetry.h"
#include <chrono>
#include <fstream>

namespace blocktype {
namespace telemetry {

// ============================================================
// PhaseGuard 实现
// ============================================================

TelemetryCollector::PhaseGuard::PhaseGuard(TelemetryCollector& C, CompilationPhase P, StringRef D)
  : Collector(C), Phase(P), Detail(D.str()),
    StartNs(getCurrentTimeNs()),
    MemoryBefore(getCurrentMemoryUsage()) {
}

TelemetryCollector::PhaseGuard::~PhaseGuard() {
  if (MovedFrom_) return;
  
  if (Collector.Enabled) {
    CompilationEvent E;
    E.Phase = Phase;
    E.Detail = Detail;
    E.StartNs = StartNs;
    E.EndNs = getCurrentTimeNs();
    E.MemoryBefore = MemoryBefore;
    E.MemoryAfter = getCurrentMemoryUsage();
    E.Success = !Failed;
    Collector.Events.push_back(std::move(E));
  }
}

// ============================================================
// TelemetryCollector 实现
// ============================================================

uint64_t TelemetryCollector::getCurrentTimeNs() {
  auto now = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
    now.time_since_epoch()
  );
  return ns.count();
}

size_t TelemetryCollector::getCurrentMemoryUsage() {
  // 简化实现：返回 0
  // 实际实现应使用平台特定 API（如 /proc/self/status）
  return 0;
}

bool TelemetryCollector::writeChromeTrace(StringRef Path) const {
  std::ofstream File(Path.str());
  if (!File.is_open()) return false;
  
  File << "{\n";
  File << "  \"traceEvents\": [\n";
  
  for (size_t i = 0; i < Events.size(); ++i) {
    const auto& E = Events[i];
    File << "    {\"ph\": \"B\", \"pid\": 0, \"tid\": 0, "
         << "\"ts\": " << E.StartNs << ", "
         << "\"name\": \"" << getPhaseName(E.Phase) << "\", "
         << "\"cat\": \"compiler\", "
         << "\"args\": {\"detail\": \"" << E.Detail << "\"}}";
    
    if (i < Events.size() - 1) File << ",";
    File << "\n";
  }
  
  File << "  ],\n";
  File << "  \"displayTimeUnit\": \"us\"\n";
  File << "}\n";
  
  return true;
}

bool TelemetryCollector::writeJSONReport(StringRef Path) const {
  std::ofstream File(Path.str());
  if (!File.is_open()) return false;
  
  File << "{\n";
  File << "  \"compilation_phases\": [\n";
  
  for (size_t i = 0; i < Events.size(); ++i) {
    const auto& E = Events[i];
    File << "    {\n";
    File << "      \"name\": \"" << getPhaseName(E.Phase) << "\",\n";
    File << "      \"detail\": \"" << E.Detail << "\",\n";
    File << "      \"duration_ms\": " << (E.EndNs - E.StartNs) / 1000000.0 << ",\n";
    File << "      \"memory_before_mb\": " << E.MemoryBefore / (1024.0 * 1024.0) << ",\n";
    File << "      \"memory_after_mb\": " << E.MemoryAfter / (1024.0 * 1024.0) << ",\n";
    File << "      \"success\": " << (E.Success ? "true" : "false") << "\n";
    File << "    }";
    
    if (i < Events.size() - 1) File << ",";
    File << "\n";
  }
  
  File << "  ]\n";
  File << "}\n";
  
  return true;
}

static const char* getPhaseName(CompilationPhase P) {
  switch (P) {
    case CompilationPhase::Frontend:       return "Frontend";
    case CompilationPhase::IRGeneration:   return "IRGeneration";
    case CompilationPhase::IRValidation:   return "IRValidation";
    case CompilationPhase::IROptimization: return "IROptimization";
    case CompilationPhase::BackendCodegen: return "BackendCodegen";
    case CompilationPhase::BackendOptimize: return "BackendOptimize";
    case CompilationPhase::CodeEmission:   return "CodeEmission";
    case CompilationPhase::Linking:        return "Linking";
  }
  return "Unknown";
}

} // namespace telemetry
} // namespace blocktype
```

### 2. CompilerInstance 类添加 TelemetryCollector 成员

```cpp
// 在 CompilerInstance.h 中添加

#include "blocktype/IR/IRTelemetry.h"

class CompilerInstance {
  // ... 现有成员 ...
  
  /// The telemetry collector (optional, for performance profiling).
  std::unique_ptr<telemetry::TelemetryCollector> Telemetry;
  
public:
  // === Telemetry 接口 ===
  
  /// Create the telemetry collector.
  void createTelemetry();
  
  /// Get the telemetry collector (may be null if not created).
  telemetry::TelemetryCollector* getTelemetry() { return Telemetry.get(); }
  const telemetry::TelemetryCollector* getTelemetry() const { return Telemetry.get(); }
  
  /// Check if telemetry is enabled.
  bool hasTelemetry() const { return Telemetry != nullptr; }
};
```

### 2. CompilerInstance::createTelemetry() 实现

```cpp
// 在 CompilerInstance.cpp 中添加

void CompilerInstance::createTelemetry() {
  if (Telemetry) return;  // 已创建
  
  Telemetry = std::make_unique<telemetry::TelemetryCollector>();
  
  // 根据编译选项决定是否启用
  if (Invocation && Invocation->getFrontendOptions().TimeReport) {
    Telemetry->enable();
  }
}
```

### 3. 编译阶段自动记录（RAII 自动计时）

```cpp
// 在 CompilerInstance::parseTranslationUnit() 中添加

TranslationUnitDecl* CompilerInstance::parseTranslationUnit() {
  telemetry::TelemetryCollector::PhaseGuard Guard;
  if (Telemetry && Telemetry->isEnabled()) {
    Guard = Telemetry->scopePhase(telemetry::CompilationPhase::Frontend, "parse");
  }
  
  // ... 现有解析逻辑 ...
  
  if (HasErrors && Telemetry && Telemetry->isEnabled()) {
    Guard.markFailed();
  }
  
  return CurrentTU;
}
```

### 4. 所有编译阶段添加遥测

| 编译阶段 | CompilationPhase 枚举值 | 调用位置 |
|---------|------------------------|---------|
| 解析 | `CompilationPhase::Frontend` | `parseTranslationUnit()` |
| 语义分析 | `CompilationPhase::Frontend` | `performSemaAnalysis()` |
| IR 生成 | `CompilationPhase::IRGeneration` | `generateLLVMIR()` |
| IR 验证 | `CompilationPhase::IRValidation` | `generateLLVMIR()` 内部 |
| IR 优化 | `CompilationPhase::IROptimization` | `runOptimizationPasses()` |
| 后端代码生成 | `CompilationPhase::BackendCodegen` | `generateObjectFile()` |
| 后端优化 | `CompilationPhase::BackendOptimize` | `runOptimizationPasses()` |
| 代码发射 | `CompilationPhase::CodeEmission` | `generateObjectFile()` |
| 链接 | `CompilationPhase::Linking` | `linkExecutable()` |

### 5. 编译选项支持

```cpp
// 在 CompilerInvocation.h 中添加（如果尚未存在）

struct FrontendOptions {
  // ... 现有选项 ...
  
  /// Enable time report (--ftime-report)
  bool TimeReport = false;
  
  /// Enable time report in JSON format (--ftime-report=json)
  bool TimeReportJSON = false;
  
  /// Enable Chrome trace output (--ftrace-compilation)
  bool TraceCompilation = false;
  
  /// Enable memory report (--fmemory-report)
  bool MemoryReport = false;
};
```

### 6. 命令行选项解析

```cpp
// 在 Driver 或 CompilerInvocation 中添加选项解析

// --ftime-report
if (Args.hasArg(OPT_ftime_report)) {
  Opts.TimeReport = true;
}

// --ftime-report=json
if (Args.hasArg(OPT_ftime_report_json)) {
  Opts.TimeReport = true;
  Opts.TimeReportJSON = true;
}

// --ftrace-compilation
if (Args.hasArg(OPT_ftrace_compilation)) {
  Opts.TraceCompilation = true;
}

// --fmemory-report
if (Args.hasArg(OPT_fmemory_report)) {
  Opts.MemoryReport = true;
}
```

---

## 实现约束

1. **零开销**：Telemetry 未创建或未启用时，编译性能不受影响
2. **线程安全**：TelemetryCollector 实例仅在主线程使用，无需加锁
3. **内存管理**：TelemetryCollector 由 CompilerInstance 持有，生命周期与 CompilerInstance 一致
4. **错误处理**：遥测失败不影响编译流程（记录日志但不中断）

---

## 验收标准

### 单元测试

```cpp
// tests/unit/Frontend/TelemetryIntegrationTest.cpp

#include "gtest/gtest.h"
#include "blocktype/Frontend/CompilerInstance.h"
#include "blocktype/IR/IRTelemetry.h"

using namespace blocktype;

TEST(TelemetryIntegration, CreateTelemetry) {
  CompilerInstance CI;
  EXPECT_FALSE(CI.hasTelemetry());
  
  CI.createTelemetry();
  EXPECT_TRUE(CI.hasTelemetry());
  EXPECT_NE(CI.getTelemetry(), nullptr);
}

TEST(TelemetryIntegration, EnableDisable) {
  CompilerInstance CI;
  CI.createTelemetry();
  
  auto* TC = CI.getTelemetry();
  EXPECT_FALSE(TC->isEnabled());
  
  TC->enable();
  EXPECT_TRUE(TC->isEnabled());
}

TEST(TelemetryIntegration, PhaseGuardTiming) {
  CompilerInstance CI;
  CI.createTelemetry();
  auto* TC = CI.getTelemetry();
  TC->enable();
  
  {
    auto Guard = TC->scopePhase(telemetry::CompilationPhase::Frontend, "test");
    // 模拟工作
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  const auto& Events = TC->getEvents();
  EXPECT_EQ(Events.size(), 1u);
  EXPECT_EQ(Events[0].Phase, telemetry::CompilationPhase::Frontend);
  EXPECT_TRUE(Events[0].Success);
  EXPECT_GT(Events[0].EndNs - Events[0].StartNs, 0u);
}

TEST(TelemetryIntegration, PhaseGuardFailure) {
  CompilerInstance CI;
  CI.createTelemetry();
  auto* TC = CI.getTelemetry();
  TC->enable();
  
  {
    auto Guard = TC->scopePhase(telemetry::CompilationPhase::IRGeneration, "test");
    Guard.markFailed();
  }
  
  const auto& Events = TC->getEvents();
  EXPECT_EQ(Events.size(), 1u);
  EXPECT_FALSE(Events[0].Success);
}

TEST(TelemetryIntegration, ChromeTraceOutput) {
  CompilerInstance CI;
  CI.createTelemetry();
  auto* TC = CI.getTelemetry();
  TC->enable();
  
  {
    auto Guard = TC->scopePhase(telemetry::CompilationPhase::Frontend, "parse");
  }
  
  EXPECT_TRUE(TC->writeChromeTrace("/tmp/test_trace.json"));
  
  // 验证文件内容
  std::ifstream File("/tmp/test_trace.json");
  std::string Content((std::istreambuf_iterator<char>(File)),
                       std::istreambuf_iterator<char>());
  EXPECT_TRUE(Content.find("\"traceEvents\"") != std::string::npos);
  EXPECT_TRUE(Content.find("Frontend") != std::string::npos);
}
```

### 集成测试

```bash
# V1: 编译时启用时间报告
# blocktype --ftime-report test.cpp -o test
# 输出包含编译阶段耗时统计

# V2: 输出 JSON 格式时间报告
# blocktype --ftime-report=json test.cpp -o test
# 输出 JSON 格式的编译阶段耗时

# V3: 输出 Chrome Trace
# blocktype --ftrace-compilation test.cpp -o test
# 生成 /tmp/blocktype_trace.json，可在 chrome://tracing 可视化

# V4: 内存报告
# blocktype --fmemory-report test.cpp -o test
# 输出各阶段内存使用情况
```

### 预期输出示例

**--ftime-report 输出**:
```
=== Time Report ===
Frontend (parse + sema):     125.3 ms
IR Generation:                45.2 ms
IR Validation:                 2.1 ms
IR Optimization:              38.7 ms
Backend Codegen:              89.4 ms
Code Emission:                12.3 ms
Total:                       313.0 ms
```

**--ftime-report=json 输出**:
```json
{
  "compilation_phases": [
    {
      "name": "Frontend",
      "detail": "parse + sema",
      "duration_ms": 125.3,
      "memory_before_mb": 10.2,
      "memory_after_mb": 25.6
    },
    {
      "name": "IR Generation",
      "duration_ms": 45.2,
      "memory_before_mb": 25.6,
      "memory_after_mb": 32.1
    }
  ],
  "total_duration_ms": 313.0
}
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F5 — TelemetryCollector 与 CompilerInstance 集成" && git push origin HEAD
```
