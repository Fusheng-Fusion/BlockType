# Task A-F1 优化版：DialectCapability 位掩码 + DialectLoweringPass

> **版本**：优化版 v1.0  
> **生成时间**：2026-04-25  
> **依赖任务**：A.1（IRType 体系）、A.2（IRInstruction DialectID_ 字段）、A.3（IRInstruction）  
> **输出路径**：`docs/plan/多前端后端重构方案/15-AI-Coder-任务流-PhaseA-AF1-优化版.md`

---

## 一、任务概述

### 1.1 目标

实现 IR 层的 Dialect（方言）能力系统，包含两个核心组件：

1. **DialectCapability** — 位掩码类，声明/查询后端支持的 Dialect 集合
2. **DialectLoweringPass** — IR Pass，将后端不支持的 Dialect 指令降级为基础 Core 指令

### 1.2 红线 Checklist

| # | 红线规则 | 本 Task 遵守方式 |
|---|---------|---------------|
| 1 | 架构优先 | DialectCapability 与 BackendCapability 正交，互不依赖 |
| 2 | 多前端多后端自由组合 | 不引入前端/后端硬耦合 |
| 3 | 渐进式改造 | 不改变现有接口签名，仅新增类和枚举值 |
| 4 | 现有功能不退化 | 所有现有测试通过 |
| 5 | 接口抽象优先 | DialectLoweringPass 继承 Pass 抽象基类 |
| 6 | IR 中间层解耦 | 仅涉及 IR 层，不涉及前后端 |

### 1.3 产出文件清单

| 操作 | 文件路径 | 说明 |
|------|----------|------|
| **修改** | `include/blocktype/IR/IRValue.h` | 新增 Cpp/Target/Metadata Dialect 的 Opcode 枚举值 |
| **修改** | `src/IR/IRInstruction.cpp` | 更新 OpcodeNames 数组和分类方法 |
| **修改** | `src/IR/CMakeLists.txt` | 添加新源文件 IRDialect.cpp 和 DialectLoweringPass.cpp |
| **新增** | `include/blocktype/IR/IRDialect.h` | DialectCapability 类定义 |
| **新增** | `src/IR/IRDialect.cpp` | DialectCapability 实现 |
| **新增** | `include/blocktype/IR/DialectLoweringPass.h` | DialectLoweringPass 类定义 |
| **新增** | `src/IR/DialectLoweringPass.cpp` | DialectLoweringPass 实现（降级规则表 + run 方法） |

> **注意**：`IRType.h` 和 `IRInstruction.h` **不需要修改** — DialectID 枚举和 DialectID_ 字段已分别在 Task A.1 和 A.2 中添加完毕。

---

## 二、现有代码背景分析

### 2.1 DialectID 枚举（已存在）

**文件**：`include/blocktype/IR/IRType.h:14-24`

```cpp
namespace dialect {
enum class DialectID : uint8_t {
  Core     = 0,
  Cpp      = 1,
  Target   = 2,
  Debug    = 3,
  Metadata = 4,
};
} // namespace dialect
```

**状态**：✅ 已完整定义，共 5 个方言。本 Task 无需修改。

### 2.2 IRInstruction 的 DialectID_ 字段（已存在）

**文件**：`include/blocktype/IR/IRInstruction.h:17-28`

```cpp
class IRInstruction : public User {
  Opcode Op;
  dialect::DialectID DialectID_;
  // ...
public:
  IRInstruction(Opcode O, IRType* Ty, unsigned ID,
                dialect::DialectID D = dialect::DialectID::Core, StringRef N = "")
    : User(ValueKind::InstructionResult, Ty, ID, N),
      Op(O), DialectID_(D), Pred_(0), Parent(nullptr) {}

  Opcode getOpcode() const { return Op; }
  dialect::DialectID getDialect() const { return DialectID_; }
  // ...
};
```

**状态**：✅ 字段名 `DialectID_`，通过构造函数参数 `D` 初始化（默认 Core），getter 为 `getDialect()`。本 Task 无需修改。

### 2.3 Opcode 枚举（现状 vs 需求）

**文件**：`include/blocktype/IR/IRValue.h:31-47`

当前已定义的 Opcode（按编号区间）：

| 区间 | 值 | Opcode 列表 |
|------|---|------------|
| 终结器 | 0-6 | Ret, Br, CondBr, Switch, **Invoke**, Unreachable, **Resume** |
| 整数运算 | 16-22 | Add, Sub, Mul, UDiv, SDiv, URem, SRem |
| 浮点运算 | 32-36 | FAdd, FSub, FMul, FDiv, FRem |
| 位运算 | 48-53 | Shl, LShr, AShr, And, Or, Xor |
| 内存操作 | 64-69 | Alloca, Load, Store, GEP, Memcpy, Memset |
| 类型转换 | 80-91 | Trunc, ZExt, SExt, FPTrunc, FPExt, FPToSI, FPToUI, SIToFP, UIToFP, PtrToInt, IntToPtr, BitCast |
| 比较 | 96-97 | ICmp, FCmp |
| 调用 | 112 | Call |
| SSA | 128-134 | Phi, Select, ExtractValue, InsertValue, ExtractElement, InsertElement, ShuffleVector |
| 调试 | 144-146 | DbgDeclare, DbgValue, DbgLabel |
| FFI | 160-163 | FFICall, FFICheck, FFICoerce, FFIUnwind |
| 原子 | 176-180 | AtomicLoad, AtomicStore, AtomicRMW, AtomicCmpXchg, Fence |

降级规则表 10 条中涉及的 Opcode 对照：

| 规则 # | 需要 Opcode | 当前状态 | 处理方式 |
|--------|-----------|---------|---------|
| 1 | Invoke | ✅ 已存在 (值=4) | 直接使用 |
| 2 | Resume | ✅ 已存在 (值=6) | 直接使用 |
| 3 | DynamicCast | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 4 | VtableDispatch | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 5 | RTTITypeid | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 6 | TargetIntrinsic | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 7 | MetaInlineAlways | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 8 | MetaInlineNever | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 9 | MetaHot | ❌ 不存在 | **需新增** Opcode 枚举值 |
| 10 | MetaCold | ❌ 不存在 | **需新增** Opcode 枚举值 |

### 2.4 Pass 基类（已存在）

**文件**：`include/blocktype/IR/IRPass.h`

```cpp
class Pass {
public:
  virtual ~Pass() = default;
  virtual StringRef getName() const = 0;
  virtual bool run(IRModule& M) = 0;
};
```

**状态**：✅ 接口清晰。DialectLoweringPass 继承此基类，实现 `getName()` 和 `run(IRModule&)`。

### 2.5 IRModule 接口

**文件**：`include/blocktype/IR/IRModule.h`

关键接口：
- `getFunctions()` — 返回 `std::vector<std::unique_ptr<IRFunction>>&`
- `getRequiredFeatures()` / `addRequiredFeature(IRFeature)` — 已有 Feature 位掩码机制
- `getTypeContext()` — 获取 IRTypeContext

IRFeature 枚举中已有 `DynamicCast = 1 << 9` 和 `VirtualDispatch = 1 << 10`，对应降级规则 3、4。

### 2.6 IRBuilder 接口

**文件**：`include/blocktype/IR/IRBuilder.h`

IRBuilder 提供了创建各类指令的方法。DialectLoweringPass 的降级逻辑将使用 IRBuilder 创建替代指令。关键方法：
- `createCall()` — 用于 Invoke→Call 降级
- `createLoad()` / `createGEP()` — 用于 VtableDispatch 降级
- `insertHelper()` — private，需要通过 public 方法间接使用

> **设计约束**：IRBuilder 的 `insertHelper` 是 private 的，降级 Pass 只能通过 IRBuilder 的 public create* 方法创建指令。如果降级逻辑需要更底层的控制（如创建自定义指令），可能需要在降级 Pass 中直接构造 IRInstruction。

---

## 三、详细设计

### 3.1 新增 Opcode 枚举值

**修改文件**：`include/blocktype/IR/IRValue.h`

在现有 Opcode 枚举中新增 8 个值，分配到三个新区间，与现有区间无冲突：

```cpp
enum class Opcode : uint16_t {
  // === 现有值（不修改） ===
  Ret = 0, Br = 1, CondBr = 2, Switch = 3, Invoke = 4, Unreachable = 5, Resume = 6,
  Add = 16, Sub = 17, Mul = 18, UDiv = 19, SDiv = 20, URem = 21, SRem = 22,
  FAdd = 32, FSub = 33, FMul = 34, FDiv = 35, FRem = 36,
  Shl = 48, LShr = 49, AShr = 50, And = 51, Or = 52, Xor = 53,
  Alloca = 64, Load = 65, Store = 66, GEP = 67, Memcpy = 68, Memset = 69,
  Trunc = 80, ZExt = 81, SExt = 82, FPTrunc = 83, FPExt = 84,
  FPToSI = 85, FPToUI = 86, SIToFP = 87, UIToFP = 88,
  PtrToInt = 89, IntToPtr = 90, BitCast = 91,
  ICmp = 96, FCmp = 97,
  Call = 112,
  Phi = 128, Select = 129, ExtractValue = 130, InsertValue = 131,
  ExtractElement = 132, InsertElement = 133, ShuffleVector = 134,
  DbgDeclare = 144, DbgValue = 145, DbgLabel = 146,
  FFICall = 160, FFICheck = 161, FFICoerce = 162, FFIUnwind = 163,
  AtomicLoad = 176, AtomicStore = 177, AtomicRMW = 178, AtomicCmpXchg = 179, Fence = 180,

  // === 新增：Cpp Dialect 指令（区间 192-199）===
  DynamicCast    = 192,  // C++ dynamic_cast<T>(v)
  VtableDispatch = 193,  // C++ 虚表分派 vtable.dispatch(obj, idx)
  RTTITypeid     = 194,  // C++ typeid(expr)

  // === 新增：Target Dialect 指令（区间 200-207）===
  TargetIntrinsic = 200, // 目标平台特定 intrinsic

  // === 新增：Metadata Dialect 指令（区间 208-215）===
  MetaInlineAlways = 208, // 强制内联
  MetaInlineNever  = 209, // 禁止内联
  MetaHot          = 210, // 热点标记
  MetaCold         = 211, // 冷点标记
};
```

**区间分配原则**：
- 192-199：Cpp Dialect（最多 8 个，当前 3 个）
- 200-207：Target Dialect（最多 8 个，当前 1 个）
- 208-215：Metadata Dialect（最多 8 个，当前 4 个）
- 每个区间预留空间，方便后续扩展

**影响范围**：
- `IRInstruction::isTerminator()` / `isBinaryOp()` / `isCast()` / `isMemoryOp()` / `isComparison()` — 这些方法基于数值区间判断，新 Opcode 不落入任何现有区间，返回 false（正确行为）
- `IRInstruction::print()` 中的 `OpcodeNames[]` 数组 — **必须扩展**以包含新 Opcode 的名称

### 3.2 IRDialect.h — DialectCapability 类

**新增文件**：`include/blocktype/IR/IRDialect.h`

```cpp
#ifndef BLOCKTYPE_IR_IRDIALECT_H
#define BLOCKTYPE_IR_IRDIALECT_H

#include <cstdint>

#include "blocktype/IR/IRType.h"  // dialect::DialectID

namespace blocktype {
namespace ir {
namespace dialect {

/// DialectCapability — 位掩码类，表示一个后端支持的 Dialect 集合。
/// 与 BackendCapability 正交：BackendCapability 描述后端的平台能力，
/// DialectCapability 描述后端能处理的 IR Dialect 类型。
///
/// 使用方式：
///   DialectCapability Cap;
///   Cap.declareDialect(DialectID::Core);
///   Cap.declareDialect(DialectID::Cpp);
///   if (!Cap.hasDialect(DialectID::Target)) { ... 不支持 Target ... }
class DialectCapability {
  uint32_t SupportedDialects_ = 0;

public:
  /// 声明支持某个 Dialect
  void declareDialect(DialectID D) {
    SupportedDialects_ |= (1u << static_cast<uint8_t>(D));
  }

  /// 查询是否支持某个 Dialect
  bool hasDialect(DialectID D) const {
    return (SupportedDialects_ & (1u << static_cast<uint8_t>(D))) != 0;
  }

  /// 查询是否支持所有必需的 Dialect（位掩码形式）
  bool supportsAll(uint32_t Required) const {
    return (SupportedDialects_ & Required) == Required;
  }

  /// 返回不支持的 Dialect 位掩码
  uint32_t getUnsupported(uint32_t Required) const {
    return Required & ~SupportedDialects_;
  }

  /// 返回完整的能力位掩码
  uint32_t getSupportedMask() const { return SupportedDialects_; }
};

/// 预定义的后端 Dialect 能力
namespace BackendDialectCaps {

/// LLVM 后端：支持全部 5 种 Dialect
inline DialectCapability LLVM() {
  DialectCapability Cap;
  Cap.declareDialect(DialectID::Core);
  Cap.declareDialect(DialectID::Cpp);
  Cap.declareDialect(DialectID::Target);
  Cap.declareDialect(DialectID::Debug);
  Cap.declareDialect(DialectID::Metadata);
  return Cap;
}

/// Cranelift 后端：仅支持 Core + Debug
inline DialectCapability Cranelift() {
  DialectCapability Cap;
  Cap.declareDialect(DialectID::Core);
  Cap.declareDialect(DialectID::Debug);
  return Cap;
}

} // namespace BackendDialectCaps

} // namespace dialect
} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRDIALECT_H
```

**设计说明**：
- `SupportedDialects_` 使用 `uint32_t`，可表示最多 32 种 Dialect（当前仅 5 种，绰绰有余）
- 与 `IRModule` 中已有的 `IRFeature` 位掩码模式保持一致
- `BackendDialectCaps` 命名空间提供预定义的工厂函数，避免硬编码
- `IRDialect.cpp` 实现文件内容较少（大部分逻辑在头文件中 inline），但为保持一致性仍然创建

### 3.3 IRDialect.cpp — 实现

**新增文件**：`src/IR/IRDialect.cpp`

```cpp
#include "blocktype/IR/IRDialect.h"

// DialectCapability 的所有方法均为 inline，定义在头文件中。
// 此文件预留用于未来需要非 inline 实现的场景（如序列化、调试输出等）。
```

> 当前 DialectCapability 的方法全部在头文件中 inline 实现，`IRDialect.cpp` 作为占位文件存在，确保 CMake 构建不出错。

### 3.4 DialectLoweringPass.h — 类定义

**新增文件**：`include/blocktype/IR/DialectLoweringPass.h`

```cpp
#ifndef BLOCKTYPE_IR_DIALECTLOWERINGPASS_H
#define BLOCKTYPE_IR_DIALECTLOWERINGPASS_H

#include <functional>

#include "blocktype/IR/IRDialect.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRPass.h"

namespace blocktype {
namespace ir {

class IRBuilder;

/// DialectLoweringPass — 将后端不支持的 Dialect 指令降级为 Core 指令。
///
/// 工作流程：
/// 1. 遍历 IRModule 中所有函数的所有基本块的所有指令
/// 2. 对每条指令检查其 DialectID 是否在后端的 DialectCapability 中
/// 3. 如果不支持，查找降级规则表
/// 4. 执行降级：将原指令替换为等效的 Core 指令序列
/// 5. 返回 true 表示 IR 被修改，false 表示无修改
class DialectLoweringPass : public Pass {
public:
  /// 构造时传入后端的 Dialect 能力
  explicit DialectLoweringPass(
      dialect::DialectCapability BackendCap = dialect::BackendDialectCaps::LLVM())
    : BackendCap_(BackendCap) {}

  StringRef getName() const override { return "dialect-lowering"; }
  bool run(IRModule& M) override;

private:
  dialect::DialectCapability BackendCap_;

  /// 降级规则：描述如何将一条 Dialect 指令转换为 Core 指令
  struct LoweringRule {
    dialect::DialectID SourceDialect;   // 源方言
    Opcode SourceOpcode;                // 源操作码
    dialect::DialectID TargetDialect;   // 目标方言（通常为 Core）
    /// 降级函数：接收原指令和 Builder，返回 true 表示降级成功
    ///降级函数负责：构造替代指令序列、替换原指令的所有使用、移除原指令
    std::function<bool(IRInstruction&)> Lower;
  };

  /// 获取降级规则表
  static const SmallVector<LoweringRule, 16>& getLoweringRules();

  /// 判断指令是否需要降级
  bool needsLowering(const IRInstruction& I) const;

  /// 对单条指令执行降级，返回是否成功
  bool lowerInstruction(IRInstruction& I) const;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_DIALECTLOWERINGPASS_H
```

**设计要点**：
- 构造函数接受 `DialectCapability`，默认使用 LLVM（支持所有 Dialect，不降级）
- `LoweringRule::Lower` 简化为只接收 `IRInstruction&`（不传 IRBuilder），因为降级逻辑需要直接操作指令所在的基本块
- `getLoweringRules()` 返回静态规则表，避免每次 run() 都重建
- `needsLowering()` 封装判断逻辑：指令的 Dialect 不在后端能力中 → 需要降级

### 3.5 DialectLoweringPass.cpp — 实现

**新增文件**：`src/IR/DialectLoweringPass.cpp`

#### 3.5.1 降级规则表（10 条详细规范）

每条规则包含：触发条件、降级行为、前置检查、错误处理。

**规则 1：Invoke → Call（bt_cpp → bt_core）**

```
触发条件：Op == Invoke && !BackendCap.hasDialect(DialectID::Cpp)
降级行为：
  invoke @callee(args...) to label %normal unwind label %unwind
  ⟶ call @callee(args...)
  ⟶ 保留原 normal 基本块，删除 unwind 基本块（如果仅有 landingpad+resume 且无其他前驱）
  ⟶ 原 Invoke 指令的结果被 Call 指令结果替换
前置检查：确保指令的 Dialect 为 Cpp
注意事项：此规则与规则 2（Resume）配合使用，确保异常处理路径完整移除
```

**规则 2：Resume → Unreachable（bt_cpp → bt_core）**

```
触发条件：Op == Resume && !BackendCap.hasDialect(DialectID::Cpp)
降级行为：
  resume {ptr, i32} %val
  ⟶ unreachable
前置检查：确保指令的 Dialect 为 Cpp
注意事项：Resume 只出现在 unwind 路径中，当 Invoke 已被规则 1 降级后，
         Resume 所在的基本块可能已不可达。此规则作为安全兜底。
```

**规则 3：DynamicCast → Runtime Call（bt_cpp → bt_core）**

```
触发条件：Op == DynamicCast && !BackendCap.hasDialect(DialectID::Cpp)
降级行为：
  %result = dynamic_cast<T>(%obj)
  ⟶ %result = call @__bt_dynamic_cast(%obj, @typeinfo.T.src, @typeinfo.T.dst, %vtable_offset)
前置检查：需要 3 个操作数（obj, src_typeinfo, dst_typeinfo）
注意事项：
  - 引入运行时辅助函数 __bt_dynamic_cast
  - 该函数的声明通过 IRModule::getOrInsertFunction 添加
  - 如果后端不支持 DynamicCast 特性，降级后还需在调用前插入 null 检查
```

**规则 4：VtableDispatch → Load + GEP + Call（bt_cpp → bt_core）**

```
触发条件：Op == VtableDispatch && !BackendCap.hasDialect(DialectID::Cpp)
降级行为：
  %result = vtable.dispatch(%obj, %vtable_idx, args...)
  ⟶ %vptr = load ptr, ptr %obj                    ; 加载 vtable 指针
  ⟶ %fptr = gep ptr, ptr %vptr, %vtable_idx       ; 索引虚表
  ⟶ %result = call %fptr(args...)                  ; 间接调用
前置检查：需要至少 2 个操作数（obj, vtable_idx），其余为函数参数
注意事项：产生 3 条替代指令，需按顺序插入到原指令位置
```

**规则 5：RTTITypeid → Runtime Call（bt_cpp → bt_core）**

```
触发条件：Op == RTTITypeid && !BackendCap.hasDialect(DialectID::Cpp)
降级行为：
  %result = typeid(%expr)
  ⟶ %result = call @__bt_typeid(%expr)
前置检查：需要 1 个操作数（expr）
注意事项：引入运行时辅助函数 __bt_typeid
```

**规则 6：TargetIntrinsic → Function Call（bt_target → bt_core）**

```
触发条件：Op == TargetIntrinsic && !BackendCap.hasDialect(DialectID::Target)
降级行为：
  %result = target.intrinsic(%name, args...)
  ⟶ %result = call @%name(args...)
前置检查：第一个操作数为函数名（通过 ConstantFunctionRef 提供）
注意事项：目标 intrinsic 名称作为函数名直接调用，需要确保函数声明存在
```

**规则 7：MetaInlineAlways → 函数属性（bt_meta → bt_core）**

```
触发条件：Op == MetaInlineAlways && !BackendCap.hasDialect(DialectID::Metadata)
降级行为：
  meta.inline.always
  ⟶ 在所在函数上添加 FunctionAttr::AlwaysInline
  ⟶ 删除该指令
前置检查：指令必须位于某个 IRFunction 中
注意事项：这是伪指令（不产生值），降级后直接删除
```

**规则 8：MetaInlineNever → 函数属性（bt_meta → bt_core）**

```
触发条件：Op == MetaInlineNever && !BackendCap.hasDialect(DialectID::Metadata)
降级行为：
  meta.inline.never
  ⟶ 在所在函数上添加 FunctionAttr::NoInline
  ⟶ 删除该指令
前置检查：同规则 7
注意事项：同规则 7
```

**规则 9：MetaHot → 函数属性（bt_meta → bt_core）**

```
触发条件：Op == MetaHot && !BackendCap.hasDialect(DialectID::Metadata)
降级行为：
  meta.hot
  ⟶ 在所在函数上添加自定义属性 "hot"（通过 FunctionAttrs 扩展位）
  ⟶ 删除该指令
前置检查：同规则 7
注意事项：FunctionAttr 枚举当前没有 Hot/Cold，需要新增：
  Hot  = 1 << 14,
  Cold = 1 << 15,
```

**规则 10：MetaCold → 函数属性（bt_meta → bt_core）**

```
触发条件：Op == MetaCold && !BackendCap.hasDialect(DialectID::Metadata)
降级行为：
  meta.cold
  ⟶ 在所在函数上添加 FunctionAttr::Cold
  ⟶ 删除该指令
前置检查：同规则 7
注意事项：需要在 FunctionAttr 枚举中新增 Cold = 1 << 15
```

#### 3.5.2 run() 方法实现规范

```cpp
bool DialectLoweringPass::run(IRModule& M) {
  bool Changed = false;
  const auto& Rules = getLoweringRules();

  for (auto& Func : M.getFunctions()) {
    for (auto& BB : Func->getBasicBlocks()) {
      auto It = BB->begin();
      while (It != BB->end()) {
        IRInstruction* I = It->get();
        if (needsLowering(*I)) {
          bool Success = lowerInstruction(*I);
          if (Success) {
            // lowerInstruction 已处理指令删除
            // 不递增 It，因为 lowerInstruction 可能修改了迭代器位置
            Changed = true;
            continue;
          }
        }
        ++It;
      }
    }
  }
  return Changed;
}
```

#### 3.5.3 needsLowering() 实现规范

```cpp
bool DialectLoweringPass::needsLowering(const IRInstruction& I) const {
  dialect::DialectID D = I.getDialect();
  // Core 和 Debug Dialect 不需要降级
  if (D == dialect::DialectID::Core || D == dialect::DialectID::Debug)
    return false;
  return !BackendCap_.hasDialect(D);
}
```

#### 3.5.4 lowerInstruction() 实现规范

```cpp
bool DialectLoweringPass::lowerInstruction(IRInstruction& I) const {
  const auto& Rules = getLoweringRules();
  for (const auto& Rule : Rules) {
    if (I.getDialect() == Rule.SourceDialect &&
        I.getOpcode() == Rule.SourceOpcode) {
      return Rule.Lower(I);
    }
  }
  // 没有匹配的降级规则 — 报告警告但不中断
  return false;
}
```

---

## 四、现有文件修改规范

### 4.1 IRValue.h — 新增 Opcode 枚举值

**文件**：`include/blocktype/IR/IRValue.h`  
**修改位置**：`enum class Opcode : uint16_t` 内，`Fence = 180` 之后  
**修改内容**：添加 8 个新枚举值（见 3.1 节）  
**同时需要**：在 `FunctionAttr` 枚举中新增 `Hot = 1 << 14` 和 `Cold = 1 << 15`  
**验证**：确保新增值不与现有值冲突；重新编译无错误

### 4.2 IRInstruction.cpp — 更新 OpcodeNames 和分类

**文件**：`src/IR/IRInstruction.cpp`  
**修改内容**：

1. **扩展 OpcodeNames 数组**（`print()` 方法中）：
   - 在数组末尾追加新 Opcode 的名称字符串
   - 保持与枚举值的索引对应

2. **isBinaryOp() / isCast() / isMemoryOp() 无需修改**：
   - 新 Opcode 的值（192-215）不在现有任何区间内
   - 这些方法对新增 Opcode 正确返回 false

3. **可能需要新增辅助方法**：
   - `isCppDialectOp()` — 判断是否为 Cpp Dialect 操作
   - `isTargetDialectOp()` — 判断是否为 Target Dialect 操作
   - `isMetadataDialectOp()` — 判断是否为 Metadata Dialect 操作
   
   ```cpp
   bool IRInstruction::isCppDialectOp() const {
     auto V = static_cast<uint16_t>(Op);
     return V >= 192 && V <= 199;
   }
   bool IRInstruction::isTargetDialectOp() const {
     auto V = static_cast<uint16_t>(Op);
     return V >= 200 && V <= 207;
   }
   bool IRInstruction::isMetadataDialectOp() const {
     auto V = static_cast<uint16_t>(Op);
     return V >= 208 && V <= 215;
   }
   ```
   
   这些方法可选，如果不在 IRInstruction.h 中声明，也可在 DialectLoweringPass 中直接用数值比较。

### 4.3 CMakeLists.txt — 添加新源文件

**文件**：`src/IR/CMakeLists.txt`  
**修改位置**：`add_library(blocktype-ir ...)` 列表中  
**修改内容**：在 `IRVerifier.cpp` 之后追加：
```cmake
  IRDialect.cpp
  DialectLoweringPass.cpp
```

---

## 五、验收标准

### 5.1 编译通过

```bash
cd build && cmake --build . --target blocktype-ir
# 零错误、零警告
```

### 5.2 单元测试用例

#### V1: DialectCapability 位掩码基本功能

```cpp
#include "blocktype/IR/IRDialect.h"
using namespace blocktype::ir::dialect;

void test_dialect_capability() {
  // 构造并声明
  DialectCapability Cap;
  Cap.declareDialect(DialectID::Core);
  Cap.declareDialect(DialectID::Cpp);

  // 查询
  assert(Cap.hasDialect(DialectID::Core));
  assert(Cap.hasDialect(DialectID::Cpp));
  assert(!Cap.hasDialect(DialectID::Debug));
  assert(!Cap.hasDialect(DialectID::Target));
  assert(!Cap.hasDialect(DialectID::Metadata));

  // 位掩码查询
  uint32_t Required = (1u << 0) | (1u << 1);  // Core + Cpp
  assert(Cap.supportsAll(Required));
  
  uint32_t RequiredAll = 0x1F;  // 全部 5 个
  assert(!Cap.supportsAll(RequiredAll));
  assert(Cap.getUnsupported(RequiredAll) == ((1u<<2)|(1u<<3)|(1u<<4)));

  // getSupportedMask
  assert(Cap.getSupportedMask() == ((1u<<0)|(1u<<1)));
}
```

#### V2: 预定义后端能力

```cpp
void test_backend_dialect_caps() {
  auto LLVM = BackendDialectCaps::LLVM();
  assert(LLVM.hasDialect(DialectID::Core));
  assert(LLVM.hasDialect(DialectID::Cpp));
  assert(LLVM.hasDialect(DialectID::Target));
  assert(LLVM.hasDialect(DialectID::Debug));
  assert(LLVM.hasDialect(DialectID::Metadata));
  assert(LLVM.getSupportedMask() == 0x1F);

  auto Cran = BackendDialectCaps::Cranelift();
  assert(Cran.hasDialect(DialectID::Core));
  assert(Cran.hasDialect(DialectID::Debug));
  assert(!Cran.hasDialect(DialectID::Cpp));
  assert(!Cran.hasDialect(DialectID::Target));
  assert(!Cran.hasDialect(DialectID::Metadata));
}
```

#### V3: DialectLoweringPass 基本运行

```cpp
#include "blocktype/IR/DialectLoweringPass.h"
#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRModule.h"

void test_lowering_pass_invoke() {
  IRContext Ctx;
  IRModule Mod("test", Ctx.getTypeContext());
  
  // 构建一个包含 bt_cpp::Invoke 指令的函数
  auto* FT = /* ... 构造函数类型 ... */;
  auto* F = Mod.getOrInsertFunction("test_func", FT);
  auto* BB = F->addBasicBlock("entry");
  
  IRBuilder Builder(Ctx);
  Builder.setInsertPoint(BB);
  // 创建 Invoke 指令（DialectID = Cpp）
  // ... 具体 IRBuilder 可能需要扩展以支持设置 DialectID ...
  
  // 使用 Cranelift 能力（不支持 Cpp）
  DialectLoweringPass Pass(BackendDialectCaps::Cranelift());
  bool Changed = Pass.run(Mod);
  
  assert(Changed == true);
  // 验证 Invoke 已被降级为 Call
}
```

#### V4: 不需要降级时返回 false

```cpp
void test_lowering_pass_noop() {
  IRContext Ctx;
  IRModule Mod("test", Ctx.getTypeContext());
  
  // 构建仅包含 Core 指令的函数
  // ...
  
  // 即使使用 Cranelift 能力，Core 指令不需要降级
  DialectLoweringPass Pass(BackendDialectCaps::Cranelift());
  bool Changed = Pass.run(Mod);
  
  assert(Changed == false);
}
```

#### V5: LLVM 能力下不降级任何指令

```cpp
void test_lowering_pass_llvm_full() {
  IRContext Ctx;
  IRModule Mod("test", Ctx.getTypeContext());
  
  // 构建包含各种 Dialect 指令的函数
  // ...
  
  // LLVM 支持全部 Dialect，不降级
  DialectLoweringPass Pass(BackendDialectCaps::LLVM());
  bool Changed = Pass.run(Mod);
  
  assert(Changed == false);
}
```

### 5.3 新增 Opcode 验证

```cpp
void test_new_opcodes() {
  // 验证新 Opcode 枚举值正确
  assert(static_cast<uint16_t>(Opcode::DynamicCast) == 192);
  assert(static_cast<uint16_t>(Opcode::VtableDispatch) == 193);
  assert(static_cast<uint16_t>(Opcode::RTTITypeid) == 194);
  assert(static_cast<uint16_t>(Opcode::TargetIntrinsic) == 200);
  assert(static_cast<uint16_t>(Opcode::MetaInlineAlways) == 208);
  assert(static_cast<uint16_t>(Opcode::MetaInlineNever) == 209);
  assert(static_cast<uint16_t>(Opcode::MetaHot) == 210);
  assert(static_cast<uint16_t>(Opcode::MetaCold) == 211);
}
```

### 5.4 现有测试不退化

```bash
cd build && ctest --output-on-failure -R blocktype
# 所有现有测试通过
```

---

## 六、实现顺序建议

为遵循"渐进式改造"原则，建议按以下顺序实现：

| 步骤 | 内容 | 可编译/可测试 |
|------|------|-------------|
| 1 | 新增 8 个 Opcode 枚举值（IRValue.h） | ✅ 编译通过 |
| 2 | 更新 OpcodeNames 数组（IRInstruction.cpp） | ✅ print() 输出正确 |
| 3 | 新增 FunctionAttr::Hot/Cold（IRValue.h） | ✅ 编译通过 |
| 4 | 新增 IRDialect.h/IRDialect.cpp + CMakeLists.txt | ✅ DialectCapability 可用 |
| 5 | 新增 DialectLoweringPass.h/DialectLoweringPass.cpp + CMakeLists.txt | ✅ Pass 可用 |
| 6 | 实现规则 1-2（Invoke/Resume 降级）+ 单元测试 | ✅ 可测试 |
| 7 | 实现规则 3-6（Cpp/Target Dialect 降级）+ 单元测试 | ✅ 可测试 |
| 8 | 实现规则 7-10（Metadata Dialect 降级）+ 单元测试 | ✅ 可测试 |
| 9 | 运行全量测试 | ✅ 无退化 |

每一步完成后都必须编译通过，步骤 6-8 每步都应提交一次可工作的中间状态。

---

## 七、风险与注意事项

### 7.1 IRBuilder 的 DialectID 支持限制

当前 IRBuilder 的 `create*` 方法没有 DialectID 参数，所有通过 IRBuilder 创建的指令默认 Dialect 为 Core。降级 Pass 创建的替代指令（Core 指令）不需要设置 DialectID，因此这不是问题。

但如果未来前端需要通过 IRBuilder 创建 Cpp/Target/Metadata Dialect 的指令，需要在 IRBuilder 中新增带 DialectID 参数的重载。这属于后续 Task 的范围。

### 7.2 新 Opcode 的 print() 输出

`IRInstruction::print()` 中的 `OpcodeNames[]` 是静态数组，索引直接对应枚举值。新增 8 个枚举值（最大 211）意味着数组需要扩展到至少 212 个元素。中间的空位（181-191、195-199 等）填充空字符串 `""`。

### 7.3 Metadata 指令的语义特殊性

规则 7-10 的 Metadata 指令是"伪指令"——它们不产生值，不参与数据流。降级方式是将其语义转移到所在函数的属性上，然后删除指令。这与规则 1-6（产生值的指令替换）不同，实现时需注意区分。

### 7.4 迭代安全性

降级过程会修改正在遍历的基本块（插入新指令、删除旧指令）。实现时必须使用安全的迭代方式（如先收集需要降级的指令，再统一处理），避免迭代器失效。

---

> **Git 提交提醒**：本 Task 完成后，立即执行：
> ```bash
> git add -A && git commit -m "feat(A): 完成 Task A-F1：DialectCapability 位掩码 + DialectLoweringPass"
> ```

