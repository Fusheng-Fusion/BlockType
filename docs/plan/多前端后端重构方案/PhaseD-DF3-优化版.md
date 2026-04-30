# Phase D-F3 — 后端管线模块化 — CodeEmitter/FrameLowering/TargetLowering 接口（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **`CodeEmitter::emit()` 的 `const TargetMachine& TM` 参数无定义**：
   - 整个 Backend 层没有 `TargetMachine` 抽象类型。
   - `llvm::TargetMachine` 是 LLVM 的类，不应在 Backend 抽象接口中使用。
   - **修复**：定义一个 `blocktype::backend::TargetMachineInfo` 轻量级 struct，封装目标平台信息（Triple、CPU、Features），或使用已有的 `BackendOptions`。建议改为 `const BackendOptions& Opts`。

2. **`FrameLowering::lower()` 的 `const ABIInfo& ABI` 参数来源不明**：
   - `ABIInfo` 定义在 `include/blocktype/CodeGen/ABIInfo.h` 中，属于 CodeGen 层，不在 Backend 层。
   - Backend 层不应依赖 CodeGen 层（依赖方向错误：CodeGen 是旧管线组件，Backend 是新管线）。
   - **修复**：定义 `blocktype::backend::ABIInfo` 或改用 `BackendOptions` 中的信息。建议在 Backend 层定义一个轻量级 `TargetABIInfo` struct。

2. **Spec 的后端管线执行序列与 D-F1/D-F2 的新类型有冲突**：
   - 步骤 1-6 引用了 `InstructionSelector`、`TargetLowering`、`FrameLowering`、`RegisterAllocator`、`DebugInfoEmitter`、`CodeEmitter`。
   - 但 `BackendBase::emitObject()` 的签名是 `bool emitObject(ir::IRModule& IRModule, StringRef OutputPath)`。
   - 新管线步骤 1-6 应在 `LLVMBackend::emitObject()` 内部调用（或作为后端子组件），而不是改变 `BackendBase` 的接口。
   - **修复**：明确这些子组件是 `LLVMBackend` 的内部实现细节，不改变 `BackendBase` 公共接口。

### P2 — 建议修复
1. **步骤 5 和 6 的并行约束难以在同步代码中实现**：Spec 说"步骤5可与步骤6并行"，但 C++ 标准库的并行支持有限。建议简化为顺序执行。
2. **`TargetLowering::lower()` 与 `InstructionSelector::select()` 的分工不明确**：两者都接受 `IRInstruction` 输出 `TargetInstructionList`。Spec 说 `InstructionSelector` 处理 Core dialect，`TargetLowering` 处理非 Core dialect——但这个分工应在接口注释中明确。

### P3 — 建议改进
1. 这些接口仅定义不实现，验收标准过于简单（仅"接口定义"）。建议增加编译验证。

## 依赖分析

- **前置依赖**：D-F1（InstructionSelector + TargetInstruction），D-F2（TargetFunction + TargetRegisterInfo）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Backend/CodeEmitter.h` |
| 新增 | `include/blocktype/Backend/FrameLowering.h` |
| 新增 | `include/blocktype/Backend/TargetLowering.h` |
| **暂不修改** | `src/Backend/LLVMBackend.cpp`（仅定义接口，不集成） |

## 当前代码状态

- `LLVMBackend::emitObject()` 当前直接调用 `IRToLLVMConverter` 和 LLVM API。
- 无 `CodeEmitter`、`FrameLowering`、`TargetLowering` 抽象。
- `ABIInfo` 在 `CodeGen/ABIInfo.h` 中，属于旧管线。

## 实现步骤

1. **创建 `CodeEmitter.h`**：定义抽象接口。
2. **创建 `FrameLowering.h`**：定义抽象接口。
3. **创建 `TargetLowering.h`**：定义抽象接口。
4. **不修改 `LLVMBackend.cpp`**——这些接口仅定义，不在此阶段集成。

## 接口签名（最终版）

```cpp
// === CodeEmitter.h ===
#pragma once
#include "blocktype/Backend/RegisterAllocator.h"  // TargetFunction
#include "blocktype/Backend/BackendOptions.h"
#include "blocktype/IR/ADT.h"

namespace blocktype::backend {

/// 代码发射器抽象基类
class CodeEmitter {
public:
  virtual ~CodeEmitter() = default;

  /// 发射单个函数的目标代码
  virtual bool emit(const TargetFunction& F,
                    const BackendOptions& Opts,
                    ir::raw_ostream& OS) = 0;

  /// 发射整个模块的目标代码
  virtual bool emitModule(const ir::SmallVectorImpl<TargetFunction>& Functions,
                          const BackendOptions& Opts,
                          ir::raw_ostream& OS) = 0;
};

} // namespace blocktype::backend
```

```cpp
// === FrameLowering.h ===
#pragma once
#include "blocktype/Backend/RegisterAllocator.h"

namespace blocktype::backend {

/// 目标 ABI 信息（轻量级，不依赖 CodeGen 层）
struct TargetABIInfo {
  bool IsLittleEndian = true;
  unsigned PointerSize = 8;
  unsigned StackAlignment = 16;
  unsigned MaxVectorAlignment = 16;
};

/// 栈帧降低抽象基类
class FrameLowering {
public:
  virtual ~FrameLowering() = default;

  /// 为函数插入 prologue/epilogue 代码并计算栈帧大小
  virtual void lower(TargetFunction& F,
                     const TargetRegisterInfo& TRI,
                     const TargetABIInfo& ABI) = 0;

  /// 获取函数的栈帧大小
  virtual uint64_t getStackSize(const TargetFunction& F) const = 0;
};

} // namespace blocktype::backend
```

```cpp
// === TargetLowering.h ===
#pragma once
#include "blocktype/Backend/InstructionSelector.h"
#include "blocktype/IR/IRType.h"  // ir::dialect::DialectID

namespace blocktype::backend {

/// 目标降低抽象基类
/// 负责将非 Core Dialect 的 IR 指令降低到目标指令
class TargetLowering {
public:
  virtual ~TargetLowering() = default;

  /// 将一条 IR 指令降低到目标指令序列
  virtual bool lower(const ir::IRInstruction& I,
                     TargetInstructionList& Output) = 0;

  /// 检查是否支持指定 Dialect
  virtual bool supportsDialect(ir::dialect::DialectID D) const = 0;
};

} // namespace blocktype::backend
```

## 后端管线执行序列（最终版）

```
LLVMBackend::emitObject(IRModule, OutputPath)  — 内部实现细节
    ├── 1. checkCapability(IRModule)
    ├── 2. convertToLLVM(IRModule)  — 使用 IRToLLVMConverter
    ├── 3. optimizeLLVMModule(M, OptLevel)  — LLVM 优化
    └── 4. emitCode(M, OutputPath, FileType)  — LLVM 代码生成

// 未来 Phase（非 Phase D）的完整后端管线：
// BackendBase::emitObject(IRModule, OutputPath)
//     ├── 1. InstructionSelector::select(IRInst) → TargetInstructionList
//     ├── 2. TargetLowering::lower(IRInst)  (Dialect!=Core)
//     ├── 3. FrameLowering::lower(TargetFunction, TRI, ABI)
//     ├── 4. RegisterAllocator::allocate(TargetFunction, TRI)
//     ├── 5. DebugInfoEmitter::emitDebugInfo(IRModule, OS)
//     └── 6. CodeEmitter::emitModule(Functions, Opts, OS)
```

## 测试计划

```cpp
// T1: CodeEmitter 接口可编译
// 定义一个 MockCodeEmitter : public CodeEmitter
// 编译通过

// T2: FrameLowering 接口可编译
// 定义一个 MockFrameLowering : public FrameLowering
// 编译通过

// T3: TargetLowering 接口可编译
// 定义一个 MockTargetLowering : public TargetLowering
// 编译通过

// T4: TargetABIInfo 默认值
backend::TargetABIInfo ABI;
EXPECT_TRUE(ABI.IsLittleEndian);
EXPECT_EQ(ABI.PointerSize, 8u);
EXPECT_EQ(ABI.StackAlignment, 16u);
```

## 风险点

1. **与 LLVMBackend 现有实现的兼容性**：`LLVMBackend` 当前直接使用 LLVM API，这些新接口是"未来管线"的抽象。不应急于集成到 `LLVMBackend` 中。
2. **`TargetMachineInfo` vs `BackendOptions`**：选择了 `BackendOptions` 而非新类型，避免过度设计。如果后续需要更多目标信息，可再抽象。
3. **`TargetABIInfo` 与 `CodeGen::ABIInfo` 的关系**：`TargetABIInfo` 是 Backend 层的简化版本，仅包含帧降低所需的基本信息。两者独立维护。
