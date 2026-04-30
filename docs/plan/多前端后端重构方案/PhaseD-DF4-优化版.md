# Phase D-F4 — DebugInfoEmitter 接口定义 + DWARF5 发射（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **IR 调试元数据命名空间错误**：
   - Spec 的映射表写 `ir::DICompileUnit` → `llvm::DICompileUnit`。
   - 实际代码中 DI 类型在 `ir::debug` 命名空间中：`ir::debug::DICompileUnit`、`ir::debug::DIType`、`ir::debug::DISubprogram`、`ir::debug::DILocation`。
   - **修复**：所有引用改为 `ir::debug::DICompileUnit` 等。

2. **`DebugInfoEmitter` 的 `emit()` 方法与 `BackendBase::emitObject()` 的集成方式不明确**：
   - `BackendBase::emitObject()` 当前没有专门的调试信息发射步骤。
   - `LLVMBackend::emitObject()` 通过 `IRToLLVMConverter` 间接处理调试信息（C.7 已实现）。
   - **修复**：`DebugInfoEmitter` 作为独立接口定义，不改变现有 `LLVMBackend` 流程。集成留给后续 Phase。

### P2 — 建议修复
1. **DWARF5 发射器依赖 LLVM DIBuilder**：`emitDWARF5()` 的实现需要 `llvm::DIBuilder`。Spec 未提及此依赖。
   - **修复**：在约束中明确 `DWARF5Emitter` 依赖 `LLVM DebugInfo` 库。

2. **`emitCodeView()` 为远期任务**：Spec 标注"Windows 后端（远期）"，但接口已定义。建议保留接口定义，不实现。

3. **`raw_ostream` 类型需限定**：Spec 写 `raw_ostream& OS`，应为 `ir::raw_ostream&` 或 `llvm::raw_ostream&`。
   - **修复**：使用 `ir::raw_ostream&`（即 `llvm::raw_ostream` 的 using 声明）。

### P3 — 建议改进
1. `DebugInfoEmitter` 可以用策略模式：由 `BackendOptions::DebugInfoFormat` 决定调用 `emitDWARF5()` 还是 `emitDWARF4()`。

## 依赖分析

- **前置依赖**：A-F5（IRDebugMetadata）— `ir::debug::DICompileUnit` 等类型已存在
- **隐含依赖**：LLVM DebugInfo 库（DIBuilder）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Backend/DebugInfoEmitter.h` |
| 新增 | `src/Backend/DWARF5Emitter.cpp` |

## 当前代码状态

- `ir::debug::DICompileUnit`、`ir::debug::DIType`、`ir::debug::DISubprogram`、`ir::debug::DILocation` 已在 `include/blocktype/IR/IRDebugMetadata.h` 中定义。
- `LLVMBackend` 的 C.7 任务已通过 `IRToLLVMConverter` 处理调试信息。
- 无独立的 `DebugInfoEmitter` 抽象。

## 实现步骤

1. **创建 `include/blocktype/Backend/DebugInfoEmitter.h`**：
   - 定义 `DebugInfoEmitter` 抽象基类
   - 定义 `DWARF5Emitter` 具体类

2. **创建 `src/Backend/DWARF5Emitter.cpp`**：
   - 实现 `DWARF5Emitter::emit()` 和 `emitDWARF5()` 的 stub（仅框架，核心实现留后续 Phase）
   - `emitDWARF4()` 和 `emitCodeView()` 返回 false（未实现）

## 接口签名（最终版）

```cpp
#pragma once
#include "blocktype/IR/ADT.h"

namespace blocktype::ir { class IRModule; }

namespace blocktype::backend {

/// 调试信息发射器抽象基类
class DebugInfoEmitter {
public:
  virtual ~DebugInfoEmitter() = default;

  /// 发射调试信息（默认格式由 BackendOptions::DebugInfoFormat 决定）
  virtual bool emit(const ir::IRModule& M, ir::raw_ostream& OS) = 0;

  /// 发射 DWARF5 格式调试信息
  virtual bool emitDWARF5(const ir::IRModule& M, ir::raw_ostream& OS) = 0;

  /// 发射 DWARF4 格式调试信息
  virtual bool emitDWARF4(const ir::IRModule& M, ir::raw_ostream& OS) = 0;

  /// 发射 CodeView 格式调试信息（Windows，远期）
  virtual bool emitCodeView(const ir::IRModule& M, ir::raw_ostream& OS) = 0;
};

/// DWARF5 调试信息发射器
class DWARF5Emitter : public DebugInfoEmitter {
public:
  bool emit(const ir::IRModule& M, ir::raw_ostream& OS) override;
  bool emitDWARF5(const ir::IRModule& M, ir::raw_ostream& OS) override;
  bool emitDWARF4(const ir::IRModule& M, ir::raw_ostream& OS) override;
  bool emitCodeView(const ir::IRModule& M, ir::raw_ostream& OS) override;
};

} // namespace blocktype::backend
```

## IR 调试元数据→LLVM 调试信息映射表（修正版）

| IR 调试信息 | LLVM 调试信息 | 文件位置 |
|-------------|--------------|---------|
| `ir::debug::DICompileUnit` | `llvm::DICompileUnit` | IRDebugMetadata.h → LLVM DIBuilder |
| `ir::debug::DIType` | `llvm::DIType` | 同上 |
| `ir::debug::DISubprogram` | `llvm::DISubprogram` | 同上 |
| `ir::debug::DILocation` | `llvm::DILocation` | 同上 |

## 测试计划

```cpp
// T1: DebugInfoEmitter 接口可编译
// 定义 MockDebugInfoEmitter : public DebugInfoEmitter
// 编译通过

// T2: DWARF5Emitter 创建
auto Emitter = std::make_unique<backend::DWARF5Emitter>();
EXPECT_NE(Emitter, nullptr);

// T3: DWARF5Emitter::emitDWARF4() 返回 false（未实现）
ir::IRModule* M = nullptr;  // 测试用空 module
// 注意：实际测试需要构造有效 IRModule
// 此测试验证接口可用性

// T4: DebugInfoEmitter 通过工厂创建
// 可选：添加 DebugInfoEmitterFactory
```

## 风险点

1. **`DWARF5Emitter` 的实际实现复杂度高**：DWARF5 格式需要深入理解 LLVM DIBuilder API。此阶段仅提供 stub。
2. **`IRModule` 中的调试信息提取**：需要 `IRModule` 暴露 `getDebugMetadata()` 接口。需确认此接口是否存在。
3. **与 C.7 的重叠**：`IRToLLVMConverter` 已实现调试信息转换（C.7），`DebugInfoEmitter` 是独立的发射器。两者职责需明确区分：前者在 IR→LLVM 转换时处理，后者在纯后端管线中使用。
