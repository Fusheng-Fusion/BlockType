# Phase D-F2 — RegisterAllocator 接口定义 + TargetFunction + TargetRegisterInfo + RegAllocFactory（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **`TargetFunction` 引用了未定义的 `TargetBasicBlock` 和 `TargetFrameInfo`**：
   - Spec 中 `TargetFunction` 类持有 `std::vector<TargetBasicBlock> Blocks` 和 `TargetFrameInfo FrameInfo`。
   - 这两个类型在整个代码库中**不存在**。
   - **修复**：必须在此任务中**先定义** `TargetBasicBlock` 和 `TargetFrameInfo`。建议在 `RegisterAllocator.h` 中定义（或单独头文件）。

2. **`IRFunctionType*` 裸指针生命周期不明确**：
   - Spec 写 `ir::IRFunctionType* Signature;`（裸指针）。
   - `IRFunctionType` 的生命周期由 `IRTypeContext` 管理。需确保 `TargetFunction` 不持有 `IRFunctionType` 的所有权。
   - **修复**：在注释中说明 `Signature` 是非拥有指针。

### P2 — 建议修复
1. **`GreedyRegAlloc.cpp` 和 `BasicRegAlloc.cpp` 的实现程度不明确**：
   - Spec 标注 Phase D 实现 `Greedy` 和 `Basic` 策略，但未给出具体实现要求。
   - **修复**：此任务仅提供接口定义和 stub 实现（返回成功），完整实现在后续 Phase。

2. **`TargetRegisterInfo` 的 `RegClassMap` 类型**：
   - Spec 使用 `DenseMap<unsigned, unsigned>`，需确认是 `ir::DenseMap` 还是 `llvm::DenseMap`。
   - **修复**：使用 `ir::DenseMap`（通过 `#include "blocktype/IR/ADT.h"`）。

3. **`RegisterAllocator::allocate()` 返回值语义**：
   - `bool allocate(TargetFunction& F, const TargetRegisterInfo& TRI)` 返回 bool。
   - 成功返回 true，分配失败（如寄存器不足）返回 false。需在注释中明确。

### P3 — 建议改进
1. `RegAllocStrategy` 的 `Fast` 策略标注为"远期"实现——考虑不在此任务中声明，避免空枚举值。

## 依赖分析

- **前置依赖**：C.5（LLVMBackend）
- **接口依赖**：`ir::IRFunctionType`（IRType.h），`ir::StringRef`（ADT.h）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Backend/RegisterAllocator.h` |
| 新增 | `src/Backend/RegisterAllocator.cpp` |
| 新增 | `src/Backend/GreedyRegAlloc.cpp` |
| 新增 | `src/Backend/BasicRegAlloc.cpp` |

## 当前代码状态

- `TargetBasicBlock`、`TargetFrameInfo`、`TargetFunction`、`TargetRegisterInfo` 均不存在。
- `RegisterAllocator`、`RegAllocFactory` 均不存在。
- Backend 层使用 `blocktype::backend` 命名空间。
- `ir::IRFunctionType` 已定义在 IRType.h 中。

## 实现步骤

1. **在 `RegisterAllocator.h` 中先定义前置类型**：
   ```cpp
   /// 目标基本块（含指令列表）
   class TargetBasicBlock {
     std::string Name_;
     ir::SmallVector<std::unique_ptr<TargetInstruction>, 16> Instructions_;
   public:
     // ...
   };

   /// 目标栈帧信息
   struct TargetFrameInfo {
     uint64_t StackSize = 0;
     uint64_t LocalVarOffset = 0;
     unsigned NumSpillSlots = 0;
     // ...
   };
   ```

2. **定义 `TargetFunction`**：持有基本块列表和栈帧信息。

3. **定义 `TargetRegisterInfo`**：描述目标平台的寄存器信息。

4. **定义 `RegisterAllocator` 抽象基类**。

5. **定义 `RegAllocFactory` 工厂类**。

6. **实现 `GreedyRegAlloc`**（stub：创建 `TargetFunction` 的空操作，返回 true）。

7. **实现 `BasicRegAlloc`**（stub：同上）。

## 接口签名（最终版）

```cpp
#pragma once
#include "blocktype/IR/ADT.h"
#include "blocktype/Backend/InstructionSelector.h"  // TargetInstruction
#include <string>
#include <vector>

namespace blocktype::backend {

/// 寄存器分配策略
enum class RegAllocStrategy {
  Greedy = 0,
  Fast   = 1,
  Basic  = 2,
};

// === 前置类型定义 ===

/// 目标基本块
class TargetBasicBlock {
  std::string Name_;
  ir::SmallVector<std::unique_ptr<TargetInstruction>, 16> Instructions_;
public:
  explicit TargetBasicBlock(ir::StringRef Name = "") : Name_(Name.str()) {}
  ir::StringRef getName() const { return Name_; }
  auto& instructions() { return Instructions_; }
  const auto& instructions() const { return Instructions_; }
  void append(std::unique_ptr<TargetInstruction> TI) {
    Instructions_.push_back(std::move(TI));
  }
};

/// 目标栈帧信息
struct TargetFrameInfo {
  uint64_t StackSize = 0;
  uint64_t LocalVarOffset = 0;
  unsigned NumSpillSlots = 0;
  unsigned MaxCallFrameSize = 0;
};

/// 目标函数（后端 IR 的函数级别表示）
class TargetFunction {
  std::string Name_;
  ir::IRFunctionType* Signature_;  // 非拥有指针，生命周期由 IRTypeContext 管理
  std::vector<TargetBasicBlock> Blocks_;
  TargetFrameInfo FrameInfo_;
public:
  TargetFunction() = default;
  TargetFunction(ir::StringRef Name, ir::IRFunctionType* Sig)
    : Name_(Name.str()), Signature_(Sig) {}

  ir::StringRef getName() const { return Name_; }
  ir::IRFunctionType* getSignature() const { return Signature_; }
  std::vector<TargetBasicBlock>& getBlocks() { return Blocks_; }
  const std::vector<TargetBasicBlock>& getBlocks() const { return Blocks_; }
  TargetFrameInfo& getFrameInfo() { return FrameInfo_; }
  const TargetFrameInfo& getFrameInfo() const { return FrameInfo_; }
};

/// 目标平台寄存器信息
class TargetRegisterInfo {
  unsigned NumRegisters_ = 0;
  ir::SmallVector<unsigned, 16> CalleeSavedRegs_;
  ir::SmallVector<unsigned, 16> CallerSavedRegs_;
  ir::DenseMap<unsigned, unsigned> RegClassMap_;
public:
  unsigned getNumRegisters() const { return NumRegisters_; }
  void setNumRegisters(unsigned N) { NumRegisters_ = N; }

  ir::ArrayRef<unsigned> getCalleeSavedRegs() const { return CalleeSavedRegs_; }
  ir::ArrayRef<unsigned> getCallerSavedRegs() const { return CallerSavedRegs_; }

  void addCalleeSavedReg(unsigned Reg) { CalleeSavedRegs_.push_back(Reg); }
  void addCallerSavedReg(unsigned Reg) { CallerSavedRegs_.push_back(Reg); }

  bool isCalleeSaved(unsigned Reg) const;
  bool isCallerSaved(unsigned Reg) const;

  void setRegClass(unsigned Reg, unsigned ClassID) { RegClassMap_[Reg] = ClassID; }
  unsigned getRegClass(unsigned Reg) const;
};

// === 寄存器分配器 ===

/// 寄存器分配器抽象基类
class RegisterAllocator {
public:
  virtual ~RegisterAllocator() = default;
  virtual ir::StringRef getName() const = 0;
  virtual RegAllocStrategy getStrategy() const = 0;

  /// 为 TargetFunction 中的虚拟寄存器分配物理寄存器
  /// \returns true if allocation succeeded, false on spill failure
  virtual bool allocate(TargetFunction& F,
                        const TargetRegisterInfo& TRI) = 0;
};

/// 寄存器分配器工厂
class RegAllocFactory {
public:
  static std::unique_ptr<RegisterAllocator> create(RegAllocStrategy Strategy);
};

} // namespace blocktype::backend
```

## 测试计划

```cpp
// T1: RegAllocFactory 创建 Greedy 分配器
auto RA = backend::RegAllocFactory::create(backend::RegAllocStrategy::Greedy);
ASSERT_NE(RA, nullptr);
EXPECT_EQ(RA->getStrategy(), backend::RegAllocStrategy::Greedy);
EXPECT_EQ(RA->getName(), "greedy");

// T2: RegAllocFactory 创建 Basic 分配器
auto RB = backend::RegAllocFactory::create(backend::RegAllocStrategy::Basic);
ASSERT_NE(RB, nullptr);
EXPECT_EQ(RB->getStrategy(), backend::RegAllocStrategy::Basic);

// T3: TargetFunction 创建
backend::TargetFunction TF;
EXPECT_TRUE(TF.getName().empty());
EXPECT_EQ(TF.getBlocks().size(), 0u);
EXPECT_EQ(TF.getFrameInfo().StackSize, 0u);

// T4: TargetFunction 带名称创建
backend::TargetFunction TF2("main", nullptr);
EXPECT_EQ(TF2.getName(), "main");

// T5: TargetRegisterInfo
backend::TargetRegisterInfo TRI;
TRI.setNumRegisters(32);
EXPECT_EQ(TRI.getNumRegisters(), 32u);

// T6: Greedy 分配 stub 测试
backend::TargetFunction F;
backend::TargetRegisterInfo TRI;
TRI.setNumRegisters(16);
auto RA = backend::RegAllocFactory::create(backend::RegAllocStrategy::Greedy);
EXPECT_TRUE(RA->allocate(F, TRI));  // stub 实现，返回 true
```

## 风险点

1. **`TargetBasicBlock` 与 `ir::IRBasicBlock` 的关系**：`TargetBasicBlock` 是后端层的独立类型，不继承 IR 层类型——需确保命名不混淆。
2. **`GreedyRegAlloc`/`BasicRegAlloc` 仅为 stub**：此阶段不实现真正的寄存器分配算法，仅提供接口。真实分配逻辑依赖 `TargetInstruction` 中的虚拟寄存器信息。
3. **`IRFunctionType*` 悬空风险**：`TargetFunction` 持有裸指针，需确保 `IRTypeContext` 生命周期覆盖 `TargetFunction`。
