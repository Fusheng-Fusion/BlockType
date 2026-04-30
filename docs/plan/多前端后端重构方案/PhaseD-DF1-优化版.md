# Phase D-F1 — InstructionSelector 接口定义 + LoweringRule + TargetInstruction（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **`ir::DialectID::Core` 命名空间错误**：
   - Spec 写 `ir::DialectID::Core`，但实际代码中 `DialectID` 在 `ir::dialect::DialectID` 命名空间中。
   - 文件 `include/blocktype/IR/IRType.h` 定义：`namespace blocktype::ir { namespace dialect { enum class DialectID : uint8_t { Core = 0, Cpp = 1, ... }; } }`
   - **修复**：所有引用改为 `ir::dialect::DialectID::Core`。

2. **`TargetInstruction` 缺少默认构造函数的字段初始化**：
   - Spec 的验收标准 `TI->getMnemonic().empty()` 暗示 `Mnemonic` 需要默认初始化为空字符串。
   - Spec 的代码中 `TargetInstruction` 类使用了 `std::string Mnemonic;`（默认空），但 `SmallVector<unsigned, 4> UsedRegs;` 等也需默认初始化（SmallVector 默认空，OK）。
   - **修复**：明确字段默认值，确保 `TargetInstruction` 可默认构造。

### P2 — 建议修复
1. **`LoweringRule` 的 `SourceDialect` 字段类型**：
   - Spec 写 `ir::DialectID`，应为 `ir::dialect::DialectID`。
   - **修复**：改为 `ir::dialect::DialectID SourceDialect = ir::dialect::DialectID::Core;`

2. **`TargetInstruction` 的 `IROperands` 类型**：
   - Spec 写 `SmallVector<ir::IRValue*, 2> IROperands;` 使用裸指针，生命周期依赖 IRModule。
   - **修复**：在注释中说明 `IROperands` 中的指针仅在 IRModule 生命周期内有效。

3. **`TargetInstructionList` 类型别名**：
   - Spec 写 `using TargetInstructionList = SmallVector<std::unique_ptr<TargetInstruction>, 8>;`
   - 由于 `TargetInstruction` 在同一 namespace，需确认 `SmallVector` 是 `ir::SmallVector` 还是 `llvm::SmallVector`。
   - 实际代码中 Backend 层使用 `ir::SmallVector`（通过 `#include "blocktype/IR/ADT.h"`）。
   - **修复**：统一使用 `ir::SmallVector`。

### P3 — 建议改进
1. `InstructionSelector::loadRules()` 的 `RuleFile` 格式未定义（JSON？TableGen？自定义 DSL？）。
2. `InstructionSelector::verifyCompleteness()` 的"完整性"定义未明确——应检查所有 `Opcode` 都有对应的 `LoweringRule`。

## 依赖分析

- **前置依赖**：C.5（LLVMBackend）
- **接口依赖**：`ir::Opcode`（IRValue.h），`ir::dialect::DialectID`（IRType.h），`ir::IRValue`（IRValue.h），`ir::IRInstruction`（IRInstruction.h）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Backend/InstructionSelector.h` |
| 新增 | `src/Backend/InstructionSelector.cpp` |

## 当前代码状态

- `ir::Opcode` 枚举已定义在 `include/blocktype/IR/IRValue.h`（uint16_t 底层类型）。
- `ir::dialect::DialectID` 枚举已定义在 `include/blocktype/IR/IRType.h`。
- `ir::IRInstruction` 类已定义，包含 `getOpcode()` 和 `getDialect()` 方法。
- `InstructionSelector`、`LoweringRule`、`TargetInstruction` 均不存在。

## 实现步骤

1. **创建 `include/blocktype/Backend/InstructionSelector.h`**：
   - 定义 `LoweringRule` struct
   - 定义 `TargetInstruction` class
   - 定义 `TargetInstructionList` 类型别名
   - 定义 `InstructionSelector` 抽象基类

2. **创建 `src/Backend/InstructionSelector.cpp`**：
   - 实现 `TargetInstruction` 的访问器方法（如果需要非内联实现）
   - `InstructionSelector` 的虚析构函数实现

3. **确保编译通过**：不改动任何现有文件。

## 接口签名（最终版）

```cpp
#pragma once
#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRValue.h"       // ir::Opcode
#include "blocktype/IR/IRType.h"        // ir::dialect::DialectID
#include <string>

namespace blocktype::backend {

/// 降级规则：描述一个 IR Opcode 如何映射到目标指令模式
struct LoweringRule {
  ir::Opcode SourceOp = ir::Opcode::Ret;
  ir::dialect::DialectID SourceDialect = ir::dialect::DialectID::Core;
  std::string TargetPattern;
  std::string Condition;
  int Priority = 0;
};

/// 目标指令：一条 machine-level 指令的抽象表示
class TargetInstruction {
  std::string Mnemonic;
  ir::SmallVector<unsigned, 4> UsedRegs;
  ir::SmallVector<unsigned, 4> DefRegs;
  ir::SmallVector<ir::IRValue*, 2> IROperands;

public:
  TargetInstruction() = default;

  /// 获取指令助记符（如 "add", "mov"）
  ir::StringRef getMnemonic() const { return Mnemonic; }
  void setMnemonic(ir::StringRef M) { Mnemonic = M.str(); }

  /// 获取使用的寄存器列表
  ir::ArrayRef<unsigned> getUsedRegs() const { return UsedRegs; }
  void addUsedReg(unsigned Reg) { UsedRegs.push_back(Reg); }

  /// 获取定义的寄存器列表
  ir::ArrayRef<unsigned> getDefRegs() const { return DefRegs; }
  void addDefReg(unsigned Reg) { DefRegs.push_back(Reg); }

  /// 获取 IR 操作数（指针仅在 IRModule 生命周期内有效）
  ir::ArrayRef<ir::IRValue*> getIROperands() const { return IROperands; }
  void addIROperand(ir::IRValue* V) { IROperands.push_back(V); }
};

/// 目标指令列表
using TargetInstructionList = ir::SmallVector<std::unique_ptr<TargetInstruction>, 8>;

/// 指令选择器抽象基类
class InstructionSelector {
public:
  virtual ~InstructionSelector() = default;

  /// 为一条 IR 指令选择目标指令序列
  virtual bool select(const ir::IRInstruction& I,
                      TargetInstructionList& Output) = 0;

  /// 从文件加载降级规则表
  virtual bool loadRules(ir::StringRef RuleFile) = 0;

  /// 验证规则表完整性（所有 Opcode 都有对应规则）
  virtual bool verifyCompleteness() = 0;
};

} // namespace blocktype::backend
```

## 测试计划

```cpp
// T1: LoweringRule 创建
backend::LoweringRule R;
R.SourceOp = ir::Opcode::Add;
R.SourceDialect = ir::dialect::DialectID::Core;
R.TargetPattern = "add32 %x %y";
R.Priority = 1;
EXPECT_EQ(R.SourceOp, ir::Opcode::Add);
EXPECT_EQ(R.Priority, 1);
EXPECT_EQ(R.SourceDialect, ir::dialect::DialectID::Core);

// T2: TargetInstruction 创建
auto TI = std::make_unique<backend::TargetInstruction>();
EXPECT_TRUE(TI->getMnemonic().empty());
EXPECT_TRUE(TI->getUsedRegs().empty());
EXPECT_TRUE(TI->getDefRegs().empty());
EXPECT_TRUE(TI->getIROperands().empty());

// T3: TargetInstruction 设置值
TI->setMnemonic("add");
TI->addUsedReg(0);
TI->addDefReg(1);
EXPECT_EQ(TI->getMnemonic(), "add");
EXPECT_EQ(TI->getUsedRegs().size(), 1u);
EXPECT_EQ(TI->getDefRegs().size(), 1u);
```

## 风险点

1. **Phase A-E 无行为变化**：此任务仅定义接口，不改变 `IRToLLVMConverter` 的行为。接口实现留待后续 Phase。
2. **命名空间一致性**：Backend 层代码在 `blocktype::backend` 中，但依赖 `ir::` 类型——需确保 ADT.h 中的类型在 `ir::` 命名空间中可用。
3. **`TargetInstruction` 的拷贝语义**：包含 `unique_ptr` 容器，不可拷贝但可移动。需确保使用场景不会意外拷贝。
