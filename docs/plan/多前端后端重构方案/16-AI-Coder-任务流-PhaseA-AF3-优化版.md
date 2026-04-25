# Task A-F3 优化版：TelemetryCollector 基础框架 + PhaseGuard RAII

> **版本**：优化版 v1.0  
> **生成时间**：2026-04-25  
> **依赖任务**：无  
> **输出路径**：`docs/plan/多前端后端重构方案/16-AI-Coder-任务流-PhaseA-AF3-优化版.md`  
> **重要状态**：**代码已完整实现** — 本文档为规范参考 + 质量审查

---

## 一、任务概述

### 1.1 目标

实现编译器遥测基础设施，包含两个核心组件：

1. **TelemetryCollector** — 遥测数据收集器，记录编译各阶段的耗时和内存使用
2. **PhaseGuard** — RAII 守卫类，自动记录编译阶段的开始/结束时间和内存

支持两种输出格式：
- **Chrome Trace JSON** — 可在 `chrome://tracing` 中可视化
- **JSON 报告** — 结构化的编译时间和内存报告

### 1.2 红线 Checklist

| # | 红线规则 | 本 Task 遵守方式 |
|---|---------|---------------|
| 1 | 架构优先 | TelemetryCollector 是独立组件，与 DialectCapability/BackendCapability 正交 |
| 2 | 多前端多后端自由组合 | 不引入前端/后端耦合，纯 IR 层组件 |
| 3 | 渐进式改造 | 仅新增文件，不改变现有接口 |
| 4 | 现有功能不退化 | 所有现有测试通过 |
| 5 | 接口抽象优先 | PhaseGuard 通过 scopePhase 工厂方法创建 |
| 6 | IR 中间层解耦 | 仅涉及 IR 层遥测 |

### 1.3 产出文件清单

| 操作 | 文件路径 | 行数 | 状态 |
|------|----------|------|------|
| **已实现** | `include/blocktype/IR/IRTelemetry.h` | 72 | ✅ 完整 |
| **已实现** | `src/IR/IRTelemetry.cpp` | 153 | ✅ 完整 |
| **已实现** | `tests/unit/IR/IRTelemetryTest.cpp` | 107 | ✅ 完整 |
| **已配置** | `src/IR/CMakeLists.txt` | — | ✅ 已含 IRTelemetry.cpp |
| **已配置** | `tests/unit/IR/CMakeLists.txt` | — | ✅ 已含 IRTelemetryTest.cpp |

> **重要**：构建系统（CMakeLists.txt）已配置完毕，无需额外修改。

---

## 二、现有代码背景分析

### 2.1 命名空间设计

**实际使用的命名空间**：`blocktype::telemetry`

```cpp
// include/blocktype/IR/IRTelemetry.h:8-12
namespace blocktype {
namespace telemetry {
using blocktype::ir::SmallVector;
using blocktype::ir::StringRef;
```

**设计选择说明**：
- Telemetry 组件使用独立命名空间 `blocktype::telemetry`，而非放在 `blocktype::ir` 中
- 通过 `using` 声明导入 `SmallVector` 和 `StringRef`，避免全限定名
- 这与 `DialectCapability`（在 `blocktype::ir::dialect` 中）和 `BackendCapability`（在 `blocktype::ir` 中）形成区分
- 合理性：Telemetry 不是 IR 数据结构的一部分，而是 IR 基础设施的工具组件

### 2.2 依赖的 ADT 类型

**SmallVector**（`blocktype::ir::SmallVector<T, unsigned N>`）：
- 模板参数：`SmallVector<CompilationEvent, 64>` — 64 个内联存储的 CompilationEvent
- 使用位置：`TelemetryCollector::Events` 成员
- 完全兼容 ✅

**StringRef**（`blocktype::ir::StringRef`）：
- 用于 `PhaseGuard` 构造函数的 `Detail` 参数和 `writeChromeTrace`/`writeJSONReport` 的 `Path` 参数
- 转换方式：`D.str()` 转为 `std::string` 存储
- 完全兼容 ✅

**raw_ostream**（`blocktype::ir::raw_ostream`）：
- 现有实现使用 `std::ofstream` 而非 `raw_fd_ostream`
- 原因：`std::ofstream` 更直接，且 JSON 输出需要格式化能力（`<<` 操作符链式调用）
- 合理性：Telemetry 输出是独立于 IR 序列化的功能，使用标准库更简洁

### 2.3 参考组件风格对照

| 特性 | DialectCapability | BackendCapability | TelemetryCollector |
|------|-------------------|-------------------|-------------------|
| 命名空间 | `blocktype::ir::dialect` | `blocktype::ir` | `blocktype::telemetry` |
| 实现方式 | 全 inline（无 .cpp） | 有 .cpp | 有 .cpp |
| 头文件行数 | 71 行 | 57 行 | 72 行 |
| include 策略 | 引用 IRType.h | 引用 IRModule.h | 引用 ADT.h |
| 工厂函数 | `BackendDialectCaps::*` | `BackendCaps::*` | 无（直接构造） |

---

## 三、详细设计分析（基于现有实现）

### 3.1 类型定义

#### CompilationPhase 枚举

**文件**：`include/blocktype/IR/IRTelemetry.h:14-23`

```cpp
enum class CompilationPhase : uint8_t {
  Frontend       = 0,
  IRGeneration   = 1,
  IRValidation   = 2,
  IROptimization = 3,
  BackendCodegen = 4,
  BackendOptimize = 5,
  CodeEmission   = 6,
  Linking        = 7,
};
```

**分析**：
- 使用 `uint8_t` 底层类型，共 8 个阶段，覆盖完整编译流水线
- 值从 0 连续递增，可用作数组索引
- **建议**：添加 `static constexpr size_t PhaseCount = 8;` 方便迭代和验证

#### CompilationEvent 结构体

**文件**：`include/blocktype/IR/IRTelemetry.h:25-33`

```cpp
struct CompilationEvent {
  CompilationPhase Phase;
  std::string Detail;
  uint64_t StartNs;       // 纳秒时间戳
  uint64_t EndNs;         // 纳秒时间戳
  size_t MemoryBefore;    // 字节
  size_t MemoryAfter;     // 字节
  bool Success;
};
```

**分析**：
- `std::string Detail` — 使用 `std::string` 而非 `StringRef`，因为事件需要持久化字符串
- `uint64_t` 纳秒精度 — 足以测量亚微秒级操作
- `size_t` 内存值 — 与平台指针宽度匹配
- **可移动性**：结构体可被 `std::move`，已在 `~PhaseGuard()` 中使用

### 3.2 TelemetryCollector 类

**文件**：`include/blocktype/IR/IRTelemetry.h:35-67`

```cpp
class TelemetryCollector {
  bool Enabled = false;
  SmallVector<CompilationEvent, 64> Events;
  uint64_t CompilationStartNs;
  // ...
};
```

#### 成员变量分析

| 成员 | 类型 | 初始值 | 用途 |
|------|------|--------|------|
| `Enabled` | `bool` | `false` | 开关控制，默认关闭 |
| `Events` | `SmallVector<CompilationEvent, 64>` | 空 | 事件存储，内联 64 个事件 |
| `CompilationStartNs` | `uint64_t` | **未初始化** | 预留字段 |

#### ⚠️ 已发现问题 P1：CompilationStartNs 未初始化

`CompilationStartNs` 声明为 `uint64_t` 但：
1. **无初始化**（不像 `Enabled` 有 `= false`）
2. **从未在 `enable()` 中赋值**
3. **从未在任何方法中使用**

**影响**：这是死代码，不影响功能，但造成困惑。

**建议修复**：
- 方案 A：在 `enable()` 中初始化 `CompilationStartNs = PhaseGuard::getCurrentTimeNs();`，并在 `writeJSONReport()` 的 summary 中输出总编译时间
- 方案 B：如果不需要此字段，直接删除

#### 接口方法

| 方法 | 签名 | 实现位置 | 功能 |
|------|------|---------|------|
| `enable()` | `void enable()` | header inline | 启用收集 |
| `isEnabled()` | `bool isEnabled() const` | header inline | 查询状态 |
| `scopePhase()` | `PhaseGuard scopePhase(CompilationPhase, StringRef)` | header inline | 创建 RAII 守卫 |
| `getEvents()` | `const SmallVector<...>& getEvents() const` | header inline | 只读访问事件 |
| `clear()` | `void clear()` | header inline | 清空事件 |
| `writeChromeTrace()` | `bool writeChromeTrace(StringRef) const` | .cpp | Chrome Trace 输出 |
| `writeJSONReport()` | `bool writeJSONReport(StringRef) const` | .cpp | JSON 报告输出 |

**设计要点**：
- `scopePhase()` 返回值（非引用），PhaseGuard 不可复制但可移动 ✅
- `getEvents()` 返回 const 引用，防止外部修改 ✅
- `writeChromeTrace()`/`writeJSONReport()` 返回 bool 表示成功/失败 ✅

### 3.3 PhaseGuard RAII 类

**文件**：`include/blocktype/IR/IRTelemetry.h:43-57`（声明）
**文件**：`src/IR/IRTelemetry.cpp:60-78`（实现）

#### 构造函数

```cpp
PhaseGuard::PhaseGuard(TelemetryCollector& C, CompilationPhase P, StringRef D)
  : Collector(C), Phase(P), Detail(D.str()),
    StartNs(getCurrentTimeNs()),
    MemoryBefore(getCurrentMemoryUsage()) {}
```

**分析**：
- 记录开始时间和内存快照
- `StringRef D` 通过 `.str()` 转为 `std::string` 持有
- 构造函数在 `.cpp` 中实现（非 inline），这是正确的 — 避免在 header 中暴露 `<chrono>` 依赖

#### 析构函数

```cpp
PhaseGuard::~PhaseGuard() {
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
```

**分析**：
- 仅在 `Collector.Enabled` 为 true 时记录事件 ✅
- 未启用时不产生任何开销（仅一次 bool 检查）✅
- 使用 `std::move(E)` 避免字符串拷贝 ✅
- `markFailed()` 可标记失败，`Success` 字段反映执行状态 ✅

#### ⚠️ 已发现问题 P2：PhaseGuard 不可复制但未 delete 拷贝构造

PhaseGuard 持有 `TelemetryCollector&` 引用，默认拷贝语义会导致两个 Guard 指向同一个 Collector，析构时记录两次事件。

**当前行为**：
```cpp
auto G1 = TC.scopePhase(CompilationPhase::Frontend, "a");
auto G2 = G1;  // 编译通过！G1 和 G2 都引用 TC
// 作用域结束时，G2 先析构记录一次，G1 再析构记录一次
```

**建议修复**：
```cpp
class PhaseGuard {
  PhaseGuard(const PhaseGuard&) = delete;
  PhaseGuard& operator=(const PhaseGuard&) = delete;
  // 移动语义保留，因为 scopePhase 返回值需要移动
  PhaseGuard(PhaseGuard&&) = default;
  PhaseGuard& operator=(PhaseGuard&&) = default;
};
```

#### ⚠️ 已发现问题 P3：移动后的 PhaseGuard 析构行为

如果 PhaseGuard 被移动构造，源对象的 `Phase`/`Detail` 等字段处于"已移动"状态。如果 `Collector.Enabled` 为 true，源对象析构时仍会 push_back 一个事件（字段可能为空/默认值）。

**建议修复**：添加 `bool MovedFrom_ = false;` 标志，移动构造时设置源对象的 `MovedFrom_ = true`，析构时检查 `!MovedFrom_`。

或者更简单的方案：在析构中检查 `Phase` 是否有效（但 CompilationPhase 没有无效值），或者使用 `Optional` 语义。

#### 静态工具方法

```cpp
static uint64_t getCurrentTimeNs();    // src/IR/IRTelemetry.cpp:30-35
static size_t getCurrentMemoryUsage(); // src/IR/IRTelemetry.cpp:37-58
```

这两个方法实现在 `.cpp` 中，是 `PhaseGuard` 的私有静态方法。

---

## 四、跨平台实现分析

### 4.1 getCurrentTimeNs()

**文件**：`src/IR/IRTelemetry.cpp:30-35`

```cpp
uint64_t TelemetryCollector::PhaseGuard::getCurrentTimeNs() {
  auto Now = std::chrono::steady_clock::steady_clock::now();
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      Now.time_since_epoch()).count());
}
```

**分析**：
- 使用 `std::chrono::steady_clock` — 单调递增时钟，不受系统时间调整影响 ✅
- 转为纳秒精度 — 满足 Chrome Trace 的微秒精度需求 ✅
- 完全跨平台 — 使用 C++11 标准库 ✅

**注意**：`steady_clock::now()` 的 `time_since_epoch()` 返回的是相对于某个不透明起点的时间，不是 Unix 时间戳。这对测量相对时间差是正确的，但不能用作绝对时间。Chrome Trace 中的 `ts` 字段会被当作相对时间使用，不影响可视化。

### 4.2 getCurrentMemoryUsage()

**文件**：`src/IR/IRTelemetry.cpp:37-58`

```cpp
size_t TelemetryCollector::PhaseGuard::getCurrentMemoryUsage() {
#ifdef __APPLE__
  malloc_statistics_t Stats;
  malloc_zone_statistics(nullptr, &Stats);
  return Stats.size_in_use;
#elif defined(__linux__)
  std::ifstream ifs("/proc/self/status");
  std::string Line;
  while (std::getline(ifs, Line)) {
    if (Line.compare(0, 6, "VmRSS:") == 0) {
      size_t Val = 0;
      sscanf(Line.c_str(), "VmRSS: %zu kB", &Val);
      return Val * 1024;
    }
  }
  return 0;
#else
  return 0;
#endif
}
```

**平台覆盖分析**：

| 平台 | 实现方式 | 返回值含义 | 准确性 |
|------|---------|-----------|--------|
| macOS (`__APPLE__`) | `malloc_zone_statistics(nullptr, &Stats)` → `Stats.size_in_use` | 当前 malloc 分配的字节数 | ⚠️ 仅统计 malloc 分配，不含栈内存、内存映射等 |
| Linux (`__linux__`) | 读取 `/proc/self/status` 的 `VmRSS` | 进程驻留物理内存（KB → 字节） | ✅ 较准确的 RSS 统计 |
| 其他平台 | 返回 0 | 无信息 | ⚠️ 无内存统计 |

**⚠️ 已发现问题 P4：Linux 解析使用 `sscanf` 的 `%zu` 格式符**

`sscanf` 的 `%zu` 在某些旧版 Linux/GLIBC 上可能不被支持。但考虑到项目使用 C++17/20，目标平台的 `sscanf` 应该支持 `%zu`。

**⚠️ 已发现问题 P5：macOS 内存统计范围有限**

`malloc_zone_statistics` 只统计默认 malloc zone 的分配量，不包含：
- 栈内存
- `mmap` 直接映射的内存
- 其他 malloc zone（如 TCMalloc/JEMalloc 如果被使用）
- 代码段和数据段

如果需要更准确的内存统计，可考虑使用 `task_info` API：
```cpp
#ifdef __APPLE__
#include <mach/mach.h>
task_basic_info_data_t Info;
mach_msg_type_number_t Count = TASK_BASIC_INFO_COUNT;
task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&Info, &Count);
return Info.resident_size;  // RSS in bytes
#endif
```

**建议**：当前实现作为基础框架已足够，后续可迭代优化。

### 4.3 Chrome Trace JSON 输出

**文件**：`src/IR/IRTelemetry.cpp:82-115`

#### 输出格式规范

```json
{
  "traceEvents": [
    {"ph":"B","pid":0,"tid":0,"ts":123456,"name":"Frontend","cat":"compiler",
     "args":{"detail":"parse","memoryBefore":10485760,"memoryAfter":20971520}},
    {"ph":"E","pid":0,"tid":0,"ts":234567}
  ],
  "displayTimeUnit": "us",
  "metadata": {"compiler": "BlockType", "version": "1.0.0"}
}
```

#### 字段规范

| 字段 | 类型 | 含义 | 值域 |
|------|------|------|------|
| `ph` | string | 事件类型 | `"B"`=Begin, `"E"`=End |
| `pid` | number | 进程 ID | 固定 `0` |
| `tid` | number | 线程 ID | 固定 `0` |
| `ts` | number | 时间戳（微秒） | `StartNs / 1000` 和 `EndNs / 1000` |
| `name` | string | 阶段名称 | `"Frontend"` ~ `"Linking"` |
| `cat` | string | 分类 | 固定 `"compiler"` |
| `args.detail` | string | 详情 | 用户传入的 StringRef |
| `args.memoryBefore` | number | 阶段前内存（字节） | `MemoryBefore` |
| `args.memoryAfter` | number | 阶段后内存（字节） | `MemoryAfter` |
| `displayTimeUnit` | string | 时间单位 | 固定 `"us"` |
| `metadata.compiler` | string | 编译器名称 | 固定 `"BlockType"` |
| `metadata.version` | string | 版本号 | 固定 `"1.0.0"` |

#### 时间戳转换

```cpp
uint64_t TsBegin = E.StartNs / 1000;  // 纳秒 → 微秒
uint64_t TsEnd   = E.EndNs / 1000;
```

Chrome Trace 的 `ts` 字段单位是微秒，除以 1000 转换 ✅。

#### ⚠️ 已发现问题 P6：JSON 字符串未转义

`Detail` 和 `phaseName` 直接输出到 JSON 字符串，如果 `Detail` 包含双引号、反斜杠或控制字符，会产生无效 JSON。

**影响场景**：前端文件路径可能包含特殊字符（如 `"C:\Users\"`）。

**建议修复**：添加 JSON 字符串转义函数：
```cpp
static std::string jsonEscape(StringRef S) {
  std::string Result;
  for (char C : S) {
    switch (C) {
    case '"':  Result += "\\\""; break;
    case '\\': Result += "\\\\"; break;
    case '\n': Result += "\\n";  break;
    case '\r': Result += "\\r";  break;
    case '\t': Result += "\\t";  break;
    default:
      if (static_cast<unsigned char>(C) < 0x20)
        Result += "\\u00XX"; // 控制字符
      else
        Result += C;
    }
  }
  return Result;
}
```

### 4.4 JSON 报告输出

**文件**：`src/IR/IRTelemetry.cpp:117-149`

#### 输出格式规范

```json
{
  "events": [
    {"phase":"Frontend","detail":"parse","durationNs":123456,
     "memoryBefore":10485760,"memoryAfter":20971520,"success":true}
  ],
  "summary": {
    "totalDurationNs": 123456,
    "peakMemoryUsage": 20971520,
    "eventCount": 1
  }
}
```

#### Summary 计算逻辑

```cpp
uint64_t TotalNs = 0;
size_t MaxMemory = 0;
for (const auto& E : Events) {
  uint64_t DurNs = (E.EndNs > E.StartNs) ? (E.EndNs - E.StartNs) : 0;
  TotalNs += DurNs;
  if (E.MemoryAfter > MaxMemory) MaxMemory = E.MemoryAfter;
}
```

**分析**：
- 总耗时 = 所有阶段耗时之和 ✅
- 峰值内存 = 所有事件中 `MemoryAfter` 的最大值 ✅
- 耗时计算有溢出保护（`E.EndNs > E.StartNs` 检查）✅

---

## 五、测试用例分析

**文件**：`tests/unit/IR/IRTelemetryTest.cpp`（107 行，7 个测试）

| 测试 | 覆盖功能 | 验证点 |
|------|---------|--------|
| `PhaseGuardRAII` | RAII 自动记录 | 事件数量、Phase、Detail、Success、时间差 |
| `DisabledNoEvents` | 未启用不记录 | 事件数量为 0 |
| `MarkFailed` | 标记失败 | Success = false |
| `ChromeTraceOutput` | Chrome Trace 输出 | 文件可打开、包含关键字段 |
| `JSONReportOutput` | JSON 报告输出 | 文件可打开、包含 events/summary |
| `MultiplePhases` | 多阶段记录 | 事件数量、各阶段正确 |
| `Clear` | 清空事件 | 清空后数量为 0 |

**测试覆盖率分析**：
- ✅ 核心功能全覆盖（RAII、开关、失败标记、两种输出、多阶段、清空）
- ⚠️ 未覆盖：空 Detail 字符串
- ⚠️ 未覆盖：极短耗时（EndNs == StartNs）
- ⚠️ 未覆盖：大量事件（超过 64 个内联容量触发堆分配）
- ⚠️ 未覆盖：writeChromeTrace/writeJSONReport 文件打开失败

---

## 六、已发现问题汇总

| 编号 | 严重程度 | 问题描述 | 修复方案 |
|------|---------|---------|---------|
| P1 | 低 | `CompilationStartNs` 声明但未初始化、未使用 | 在 `enable()` 中初始化或删除字段 |
| P2 | 中 | PhaseGuard 未禁用拷贝构造/赋值 | 添加 `= delete` 声明 |
| P3 | 中 | PhaseGuard 移动后源对象析构可能记录空事件 | 添加 `MovedFrom_` 标志 |
| P4 | 低 | Linux `sscanf` 使用 `%zu` 格式 | 当前可接受，无需立即修改 |
| P5 | 低 | macOS 内存统计仅统计 malloc | 可后续优化为 `task_info` |
| P6 | 中 | JSON 字符串未转义 | 添加 `jsonEscape()` 函数 |

**建议修复优先级**：P2 > P6 > P3 > P1 > P5 > P4

---

## 七、验收标准

### 7.1 原始验收标准对照

| 标准 | 测试用例 | 状态 |
|------|---------|------|
| V1: PhaseGuard RAII 计时 | `PhaseGuardRAII` | ✅ 通过 |
| V2: Chrome Trace 输出 | `ChromeTraceOutput` | ✅ 通过 |
| 禁用时不记录事件 | `DisabledNoEvents` | ✅ 通过 |
| 失败标记 | `MarkFailed` | ✅ 通过 |
| JSON 报告输出 | `JSONReportOutput` | ✅ 通过 |
| 多阶段 | `MultiplePhases` | ✅ 通过 |
| 清空事件 | `Clear` | ✅ 通过 |

### 7.2 编译验证

```bash
cd build && cmake --build . --target blocktype-ir
# 零错误、零警告
```

### 7.3 测试验证

```bash
cd build && ctest --output-on-failure -R Telemetry
# 7 个测试全部通过
```

### 7.4 现有测试不退化

```bash
cd build && ctest --output-on-failure -R blocktype
# 所有现有测试通过
```

---

## 八、建议改进（非阻塞）

### 8.1 编译选项集成（后续 Task）

原始任务提到的编译选项，不在本 Task 范围内，记录备用：
- `--ftime-report` — 输出编译时间报告（文本格式）
- `--ftime-report=json` — 输出编译时间报告（JSON格式）
- `--ftrace-compilation` — 输出 Chrome Trace 格式
- `--fmemory-report` — 输出内存使用报告

### 8.2 未来增强方向

1. **多线程支持** — 当前 `TelemetryCollector` 非线程安全，如果编译器引入并行编译需要加锁
2. **阶段嵌套** — 当前不支持嵌套阶段（如 Frontend 内部的 Lexer/Parser），可引入 `Depth` 字段
3. **采样率控制** — 对高频事件支持采样，减少开销
4. **内存统计优化** — 使用平台特定 API 获取更准确的 RSS

---

> **Git 提交提醒**：本 Task 的代码已存在。如果执行了 P2/P6 等修复，提交格式：
> ```bash
> git add -A && git commit -m "fix(A): Task A-F3 修复：PhaseGuard 禁用拷贝 + JSON 转义 + CompilationStartNs 初始化"
> ```

