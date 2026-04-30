# Phase D Planner 预审报告

> **日期**：2026-04-30
> **角色**：Planner（bmad-po）
> **Phase**：D（管线重构）
> **Spec 来源**：`docs/plan/多前端后端重构方案/13-AI-Coder-任务流-PhaseCDE.md`

---

## 1. 执行摘要

Phase D 包含 **5 个主任务**（D.1~D.5）和 **5 个增强任务**（D-F1~D-F5）。

预审发现 **11 个 P1 级问题**（必须修复）、**13 个 P2 级问题**（建议修复）、**8 个 P3 级建议**。

关键发现：
- D.1 基本可行，但需增加 `ExplicitlySet` 标记以支撑 D.3 的 fallback 检测。
- D.2 是核心重构任务，Spec 的 `CompilerInstance` 新设计严重简化了新旧管线并存的复杂性。
- D.3 的 fallback 检测逻辑存在矛盾，与 D.1 的默认值设计冲突。
- D.4 的 `CppFrontendRegistration.cpp` **已存在**，Spec 标注为"新增"是错误的。
- D-F 系列任务引用了多个不存在的类型，需要补充定义。

---

## 2. 逐任务问题汇总

### D.1 — CompilerInvocation 新增 FrontendName/BackendName
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| P2 | Spec 未提及 `ExplicitlySet` 标记 | 新增 `bool FrontendExplicitlySet/BackendExplicitlySet` |
| P2 | 命令行解析集成方式不明 | 在 `parseCommandLine()` 中解析 `--frontend/--backend` |
| P3 | 字段直接加在 CompilerInvocation 类上，不符合现有 struct 分组风格 | 可保持，或新增 `PipelineOptions` struct |

### D.2 — CompilerInstance 重构为管线编排器
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | Spec 的 CompilerInstance 新成员列表缺少旧管线组件的保留说明 | 保留旧组件，新增新组件，`compileFile()` 中分流 |
| **P1** | `runFrontend(Filename)` 签名不完整，缺少 `IRTypeContext&` 和 `TargetLayout&` | `runFrontend()` 需先创建这些对象 |
| **P1** | Spec 未说明 `IRTypeContext`/`TargetLayout` 的创建方式 | 在 `compileFile()` 流程中明确创建步骤 |
| **P1** | `FrontendBase::compile()` 返回类型与 Spec 不一致（实际返回 `unique_ptr`） | 接口签名与实际实现一致 |
| P2 | Spec 未提及 Telemetry 的集成 | 新管线保持遥测能力 |

### D.3 — 保留旧路径作为 fallback
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | Fallback 检测逻辑矛盾——D.1 默认值使字段始终有值 | 使用 `isFrontendExplicitlySet()/isBackendExplicitlySet()` 判断 |
| P2 | 旧管线代码标记范围不明确 | 提取到 `runOldPipeline()` 方法并标记 |
| P2 | `compileAllFiles()` 也需要同步修改 | 明确 |

### D.4 — 自动注册机制
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | `CppFrontendRegistration.cpp` **已存在**，Spec 标注为"新增" | 仅创建 `LLVMBackendRegistration.cpp` |
| **P1** | Spec 的注册代码命名空间错误 | 使用 `backend::BackendRegistry`、`frontend::FrontendRegistry` |
| P2 | 未提及 `createLLVMBackend` 的声明位置 | 已在 `LLVMBackend.h` 中声明 |

### D.5 — 集成测试
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| P2 | 验收标准使用 shell 命令但框架是 GTest | 提供 GTest 测试用例 |
| P2 | 未列出测试文件产出路径 | 明确路径 |

### D-F1 — InstructionSelector 接口定义
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | `ir::DialectID::Core` 命名空间错误 | 改为 `ir::dialect::DialectID::Core` |
| P2 | `TargetInstruction` 缺少 setter 方法 | 补充 `setMnemonic()`, `addUsedReg()` 等 |

### D-F2 — RegisterAllocator 接口定义
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | `TargetFunction` 引用未定义的 `TargetBasicBlock` 和 `TargetFrameInfo` | 在此任务中先定义这两个类型 |
| P2 | `IRFunctionType*` 裸指针生命周期不明确 | 注释说明为非拥有指针 |

### D-F3 — 后端管线模块化
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | `CodeEmitter::emit()` 的 `const TargetMachine& TM` 无定义 | 改为 `const BackendOptions& Opts` |
| **P1** | `FrameLowering::lower()` 的 `const ABIInfo& ABI` 在 CodeGen 层，依赖方向错误 | 定义 `TargetABIInfo` 轻量级 struct |

### D-F4 — DebugInfoEmitter 接口定义
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | IR 调试元数据命名空间错误：`ir::DICompileUnit` 应为 `ir::debug::DICompileUnit` | 修正所有引用 |
| P2 | DWARF5 发射器依赖 LLVM DIBuilder 未提及 | 在约束中明确依赖 |

### D-F5 — BackendDiffTestIntegration 后端差分测试
| 级别 | 问题 | 修复方案 |
|------|------|---------|
| **P1** | `DifferentialTester` 类完全不存在 | 自行定义 `DiffResult`、`DiffGranularity` 等类型 |
| **P1** | `IRFuzzer` 类完全不存在 | 定义 `BackendFuzzIntegration` 的独立接口，`IRFuzzer` 留远期 |

---

## 3. 接口一致性检查

### 3.1 Spec 接口 vs Phase B/C 已实现接口

| Spec 接口 | 实际代码 | 状态 |
|-----------|---------|------|
| `BackendBase::emitObject(IRModule, StringRef)` | `bool emitObject(ir::IRModule&, ir::StringRef)` | ✅ 一致 |
| `BackendBase::getCapability()` 返回 `BackendCapability` | 返回 `ir::BackendCapability`（在 `ir::` 命名空间） | ⚠️ 命名空间差异 |
| `BackendRegistry::create(Name, Opts, Diags)` | `std::unique_ptr<BackendBase> create(ir::StringRef, const BackendOptions&, DiagnosticsEngine&)` | ✅ 一致 |
| `FrontendBase::compile()` 返回 `IRModule*` | 返回 `std::unique_ptr<ir::IRModule>` | ❌ Spec 错误 |
| `FrontendBase::compile()` 参数 `(Filename, TypeCtx, Layout)` | `(StringRef, IRTypeContext&, const TargetLayout&)` | ✅ 一致但 Spec 遗漏了后两个参数 |
| `BackendCapability` 在 `BackendBase.h` 中 | 在 `blocktype/IR/BackendCapability.h` 中 | ⚠️ 位置差异 |
| `IRFeature` 枚举 | 在 `blocktype/IR/IRModule.h` 中 | ✅ 一致 |
| `BackendFactory` 类型 | 在 `BackendBase.h` 中 | ✅ 一致 |
| `FrontendFactory` 类型 | 在 `FrontendBase.h` 中 | ✅ 一致 |
| `createLLVMBackend()` 工厂 | 在 `LLVMBackend.h` 中声明，`LLVMBackend.cpp` 中实现 | ✅ 一致 |
| `createCppFrontend()` 工厂 | 在 `CppFrontendRegistration.cpp` 中定义 | ✅ 一致 |

### 3.2 Spec 引用的不存在类型

| 类型 | Spec 任务 | 状态 |
|------|---------|------|
| `TargetBasicBlock` | D-F2 | ❌ 不存在，需在 D-F2 中定义 |
| `TargetFrameInfo` | D-F2 | ❌ 不存在，需在 D-F2 中定义 |
| `TargetMachine`（Backend 层） | D-F3 | ❌ 不存在，改为使用 `BackendOptions` |
| `ABIInfo`（Backend 层） | D-F3 | ❌ 仅存在于 CodeGen 层，需定义 Backend 层版本 |
| `DifferentialTester` | D-F5 | ❌ 完全不存在，需自行定义 |
| `IRFuzzer` | D-F5 | ❌ 完全不存在，需自行定义 |
| `ir::DialectID::Core` | D-F1 | ❌ 应为 `ir::dialect::DialectID::Core` |

---

## 4. 依赖关系验证

### 4.1 任务执行顺序（优化版）

```
D.1 (CompilerInvocation)
  ↓
D.2 (CompilerInstance 重构)
  ↓
D.3 (Fallback) ←── D.4 (自动注册) [D.4 可与 D.3 并行]
  ↓
D.5 (集成测试)

D-F1 (InstructionSelector) [独立，可与主任务并行]
D-F2 (RegisterAllocator) [依赖 D-F1 的 TargetInstruction]
D-F3 (CodeEmitter/FrameLowering/TargetLowering) [依赖 D-F1, D-F2]
D-F4 (DebugInfoEmitter) [独立，可与主任务并行]
D-F5 (BackendDiffTest) [依赖 D.5]
```

### 4.2 外部依赖验证

| 依赖 | 状态 |
|------|------|
| Phase A（IR 层） | ✅ 已完成 |
| Phase B（前端适配层） | ✅ 已完成 |
| Phase C（后端抽象层） | ✅ 已完成（commit 1b2fdb0） |
| `LLVMBackend` + `createLLVMBackend` | ✅ 已实现 |
| `CppFrontend` + `createCppFrontend` | ✅ 已实现 |
| `IRToLLVMConverter` | ✅ 已实现 |
| `CppFrontendRegistration.cpp` | ✅ 已存在 |
| `LLVMBackendRegistration.cpp` | ❌ 需创建（D.4） |
| `IRModule::getRequiredFeatures()` | ✅ 已实现 |

---

## 5. 关键架构风险

1. **新旧管线并存**：D.2~D.3 需要旧管线（AST→LLVM IR→目标代码）和新管线（Frontend→IR→Backend）在同一 `CompilerInstance` 中共存。旧管线直接使用 LLVM API，新管线通过抽象接口调用。两类组件的初始化、生命周期管理需要小心处理。

2. **IRTypeContext 生命周期**：新管线中 `FrontendBase::compile()` 需要 `IRTypeContext&`，但其所有权归属不明确。建议 `CompilerInstance` 拥有 `IRTypeContext`。

3. **BackendOptions 映射**：`CompilerInvocation` 的配置分散在 `CodeGenOpts`、`TargetOpts` 中，需正确映射到 `BackendOptions`。

4. **D-F 系列的"接口仅定义"策略**：D-F1~D-F4 仅定义接口，不改变现有行为。这降低了风险，但也意味着这些接口未经实战验证。

---

## 6. 产出清单

| 文件 | 说明 |
|------|------|
| `PhaseD-Planner-Report.md` | 本报告 |
| `PhaseD-D1-优化版.md` | D.1 优化版 task 文件 |
| `PhaseD-D2-优化版.md` | D.2 优化版 task 文件 |
| `PhaseD-D3-优化版.md` | D.3 优化版 task 文件 |
| `PhaseD-D4-优化版.md` | D.4 优化版 task 文件 |
| `PhaseD-D5-优化版.md` | D.5 优化版 task 文件 |
| `PhaseD-DF1-优化版.md` | D-F1 优化版 task 文件 |
| `PhaseD-DF2-优化版.md` | D-F2 优化版 task 文件 |
| `PhaseD-DF3-优化版.md` | D-F3 优化版 task 文件 |
| `PhaseD-DF4-优化版.md` | D-F4 优化版 task 文件 |
| `PhaseD-DF5-优化版.md` | D-F5 优化版 task 文件 |

---

## 7. 建议

1. **D.1 是最高优先级**：所有后续任务都依赖它。建议先完成 D.1 并验证命令行解析。
2. **D.2 是最大风险点**：需要仔细处理新旧管线共存。建议分两步：(a) 新增新管线方法但不改变 `compileFile()` 入口，(b) 在 D.3 中修改入口。
3. **D-F 系列建议延后**：如果时间紧张，D-F1~D-F5 可在主任务 D.1~D.5 完成后再做。它们是独立的接口定义，不影响主管线功能。
4. **D-F5 建议大幅简化**：由于 `DifferentialTester` 和 `IRFuzzer` 均不存在，D-F5 的实现成本远高于 Spec 预期。建议仅定义接口+最简单的 stub。
