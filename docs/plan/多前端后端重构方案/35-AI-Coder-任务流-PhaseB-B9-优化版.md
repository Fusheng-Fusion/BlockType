# Task B.9 IRConstantEmitter — 优化版规格

> 文档版本：优化版 v1.0（基于代码库 API 验证）
> 原始规格：`docs/plan/多前端后端重构方案/12-AI-Coder-任务流-PhaseB.md` Task B.9（约第 1030~1091 行）
> 依赖：Phase A（IRConstant 体系）— 已完成

---

## 一、代码库验证摘要（10 项修正）

| # | 原始规格 | 实际代码库 | 修正措施 |
|---|---------|-----------|---------|
| 1 | `EmitConstInt(const APSInt& Val)` | 项目无 `APSInt` 类，只有 `ir::APInt`（`IR/ADT/APInt.h`） | 改为 `EmitConstInt(const ir::APInt& Val)` |
| 2 | `EmitConstFloat(const APFloat& Val)` | `ir::APFloat`（`IR/ADT/APFloat.h`），构造函数为 `APFloat(Semantics, double)` | 改为 `EmitConstFloat(const ir::APFloat& Val)` |
| 3 | 构造函数 `IRConstantEmitter(IRTypeContext& C)` | `IRConstantInt` 等需要通过 `IRContext::create<T>()` 分配（BumpPtrAllocator） | 构造函数改为 `IRConstantEmitter(ir::IRContext& C, ir::IRTypeContext& TC)` |
| 4 | `EmitConstStruct(ArrayRef<IRConstant*> Vals)` | `IRConstantStruct` 构造函数签名 `(IRStructType*, SmallVector<IRConstant*, 16>)` | 改为 `SmallVector<ir::IRConstant*, 16>` 参数 |
| 5 | `EmitConstArray(ArrayRef<IRConstant*> Vals)` | `IRConstantArray` 构造函数签名 `(IRArrayType*, SmallVector<IRConstant*, 16>)` | 改为 `SmallVector<ir::IRConstant*, 16>` 参数 |
| 6 | 常量通过 `IRContext::create<T>()` 分配 | 确认 `IRContext::create<T>(Args...)` 存在且使用 BumpPtrAllocator（`IR/IRContext.h:36-41`） | ✅ 已确认，直接使用 |
| 7 | `EmitConstNull(IRType* T)` | `IRConstantNull(IRType* Ty)` 不需要 IRContext，但也兼容 create | 通过 `create<IRConstantNull>(T)` 统一分配 |
| 8 | `EmitConstUndef(IRType* T)` | `IRConstantUndef` 有 static `get(IRContext&, IRType*)` 且内部有缓存（`UndefCache`） | 优先使用 `IRConstantUndef::get(IRCtx, T)` 而非 create |
| 9 | 未提及 `IRConstantFunctionRef`/`IRConstantGlobalRef` | 这两个类已存在于 `IRConstant.h`，但不属于 IRConstantEmitter 职责范围 | 无需添加 |
| 10 | 验收标准中 `APFloat(3.14)` | `ir::APFloat` 构造函数为 `APFloat(Semantics::Double, 3.14)` 或 `APFloat(3.14)`（隐式 double） | 修正测试代码 |

---

## 二、产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Frontend/IRConstantEmitter.h` |
| 新增 | `src/Frontend/IRConstantEmitter.cpp` |
| 新增 | `tests/unit/Frontend/IRConstantEmitterTest.cpp` |

---

## 三、头文件依赖

```cpp
// IRConstantEmitter.h
#pragma once

#include "blocktype/IR/ADT.h"              // APInt, APFloat, SmallVector, ArrayRef
#include "blocktype/IR/IRContext.h"         // IRContext::create<T>()
#include "blocktype/IR/IRTypeContext.h"     // getIntType(), getFloatType(), getStructType(), getArrayType()
#include "blocktype/IR/IRConstant.h"        // 所有 IRConstant 子类
#include "blocktype/IR/IRType.h"            // IRIntegerType, IRFloatType, IRStructType, IRArrayType
```

不需要：
- `CodeGenModule.h`（后端无关）
- `ASTContext.h`（不依赖 AST）
- `llvm/ADT/APSInt.h`（项目不使用 LLVM APSInt）

---

## 四、类定义（精确 API）

```cpp
namespace blocktype::frontend {

class IRConstantEmitter {
  ir::IRContext& IRCtx_;
  ir::IRTypeContext& TypeCtx_;

public:
  /// 构造函数：需要 IRContext 用于常量分配，IRTypeContext 用于类型查询。
  /// 参照 IREmitExpr 的模式：Converter_.getIRContext().create<T>(...)
  explicit IRConstantEmitter(ir::IRContext& C, ir::IRTypeContext& TC)
    : IRCtx_(C), TypeCtx_(TC) {}

  //===------------------------------------------------------------------===//
  // 整数常量
  //===------------------------------------------------------------------===//

  /// 发射整数常量（APInt 版本）。
  /// BitWidth 从 APInt 自身获取。
  /// @param Val 任意位宽的整数值
  /// @return IRConstantInt* ，类型为 IRIntegerType(Val.getBitWidth())
  ir::IRConstantInt* EmitConstInt(const ir::APInt& Val);

  /// 发射整数常量（便捷版本）。
  /// 内部构造 APInt(BitWidth, Val, IsSigned)。
  /// @param Val      数值
  /// @param BitWidth 位宽（必须 > 0）
  /// @param IsSigned 是否有符号
  ir::IRConstantInt* EmitConstInt(uint64_t Val, unsigned BitWidth, bool IsSigned = false);

  //===------------------------------------------------------------------===//
  // 浮点常量
  //===------------------------------------------------------------------===//

  /// 发射浮点常量。
  /// @param Val APFloat 值（含语义信息确定位宽：16/32/64/80/128）
  ir::IRConstantFP* EmitConstFloat(const ir::APFloat& Val);

  /// 发射浮点常量（便捷版本）。
  /// @param Val      双精度值
  /// @param BitWidth 位宽（32 = float, 64 = double, 默认 64）
  ir::IRConstantFP* EmitConstFloat(double Val, unsigned BitWidth = 64);

  //===------------------------------------------------------------------===//
  // 空值 / 未定义值 / 零聚合
  //===------------------------------------------------------------------===//

  /// 发射空指针常量。
  ir::IRConstantNull* EmitConstNull(ir::IRType* T);

  /// 发射未定义值常量。使用 IRConstantUndef::get() 以利用缓存。
  ir::IRConstantUndef* EmitConstUndef(ir::IRType* T);

  /// 发射聚合零值常量（用于默认初始化）。
  ir::IRConstantAggregateZero* EmitConstAggregateZero(ir::IRType* T);

  //===------------------------------------------------------------------===//
  // 聚合常量
  //===------------------------------------------------------------------===//

  /// 发射结构体常量。
  /// @param Ty   结构体类型，必须已设置 body（非 opaque）
  /// @param Vals 各字段常量，数量和类型必须与 Ty->getElements() 匹配
  ir::IRConstantStruct* EmitConstStruct(ir::IRStructType* Ty,
                                        ir::SmallVector<ir::IRConstant*, 16> Vals);

  /// 发射结构体常量（ArrayRef 重载，内部转为 SmallVector）。
  ir::IRConstantStruct* EmitConstStruct(ir::IRStructType* Ty,
                                        ir::ArrayRef<ir::IRConstant*> Vals);

  /// 发射数组常量。
  /// @param Ty   数组类型
  /// @param Vals 各元素常量
  ir::IRConstantArray* EmitConstArray(ir::IRArrayType* Ty,
                                      ir::SmallVector<ir::IRConstant*, 16> Vals);

  /// 发射数组常量（ArrayRef 重载）。
  ir::IRConstantArray* EmitConstArray(ir::IRArrayType* Ty,
                                      ir::ArrayRef<ir::IRConstant*> Vals);

  //===------------------------------------------------------------------===//
  // 访问器
  //===------------------------------------------------------------------===//

  ir::IRContext& getIRContext() { return IRCtx_; }
  ir::IRTypeContext& getTypeContext() { return TypeCtx_; }
};

} // namespace blocktype::frontend
```

---

## 五、实现细节

### 5.1 EmitConstInt — 整数常量

```cpp
// IRConstantEmitter.cpp

ir::IRConstantInt*
IRConstantEmitter::EmitConstInt(const ir::APInt& Val) {
  ir::IRIntegerType* Ty = TypeCtx_.getIntType(Val.getBitWidth());
  return IRCtx_.create<ir::IRConstantInt>(Ty, Val);
}

ir::IRConstantInt*
IRConstantEmitter::EmitConstInt(uint64_t Val, unsigned BitWidth, bool IsSigned) {
  ir::APInt AP(BitWidth, Val, IsSigned);
  return EmitConstInt(AP);
}
```

**验证来源**：
- `IRConstantInt` 构造函数：`(IRIntegerType*, const APInt&)` 或 `(IRIntegerType*, uint64_t)` — `IRConstant.h:36-39`
- `IRTypeContext::getIntType(unsigned BitWidth)` — `IRTypeContext.h:33`
- IREmitExpr 实际用法：`Converter_.getIRContext().create<ir::IRConstantInt>(IntTy, IL->getValue().getZExtValue())` — `IREmitExpr.cpp:655-656`

### 5.2 EmitConstFloat — 浮点常量

```cpp
ir::IRConstantFP*
IRConstantEmitter::EmitConstFloat(const ir::APFloat& Val) {
  // 从 APFloat 语义确定位宽
  unsigned BitWidth = ir::APFloat::getTotalBits(Val.getSemantics());
  ir::IRFloatType* Ty = TypeCtx_.getFloatType(BitWidth);
  return IRCtx_.create<ir::IRConstantFP>(Ty, Val);
}

ir::IRConstantFP*
IRConstantEmitter::EmitConstFloat(double Val, unsigned BitWidth) {
  ir::APFloat::Semantics Sem =
      (BitWidth <= 32) ? ir::APFloat::Semantics::Float
                       : ir::APFloat::Semantics::Double;
  ir::APFloat APF(Sem, Val);
  return EmitConstFloat(APF);
}
```

**验证来源**：
- `IRConstantFP` 构造函数：`(IRFloatType*, const APFloat&)` — `IRConstant.h:52-53`
- `APFloat::Semantics` 枚举 + `APFloat(Semantics, double)` 构造 — `APFloat.h:17-23,171-175`
- `APFloat::getTotalBits(Semantics)` 静态方法 — `APFloat.h:76-85`
- IREmitExpr 实际用法：`IRCtx.create<ir::IRConstantFP>(FloatTy, APF)` — `IREmitExpr.cpp:675`

**注意**：`APFloat::getSemantics()` 需要确认是否已实现。如果未实现，可通过以下替代方案：

```cpp
// 替代方案：直接接受 IRFloatType* 参数
ir::IRConstantFP* EmitConstFloat(ir::IRFloatType* Ty, const ir::APFloat& Val);
```

### 5.3 EmitConstNull / EmitConstUndef / EmitConstAggregateZero

```cpp
ir::IRConstantNull*
IRConstantEmitter::EmitConstNull(ir::IRType* T) {
  return IRCtx_.create<ir::IRConstantNull>(T);
}

ir::IRConstantUndef*
IRConstantEmitter::EmitConstUndef(ir::IRType* T) {
  // 使用 static get() 以利用 IRContext 的 UndefCache 缓存
  return ir::IRConstantUndef::get(IRCtx_, T);
}

ir::IRConstantAggregateZero*
IRConstantEmitter::EmitConstAggregateZero(ir::IRType* T) {
  return IRCtx_.create<ir::IRConstantAggregateZero>(T);
}
```

**验证来源**：
- `IRConstantNull(IRType*)` — `IRConstant.h:63`
- `IRConstantUndef::get(IRContext&, IRType*)` — `IRConstant.h:73`，实现 `IRContext.cpp:29-31`（使用 `UndefCache`）
- `IRConstantAggregateZero(IRType*)` — `IRConstant.h:80`

### 5.4 EmitConstStruct / EmitConstArray

```cpp
ir::IRConstantStruct*
IRConstantEmitter::EmitConstStruct(ir::IRStructType* Ty,
                                    ir::SmallVector<ir::IRConstant*, 16> Vals) {
  // 断言：字段数量和类型必须匹配
  assert(Ty->getNumFields() == Vals.size() && "Struct field count mismatch");
  for (unsigned i = 0; i < Vals.size(); ++i) {
    assert(Vals[i]->getType() == Ty->getFieldType(i) &&
           "Struct field type mismatch");
  }
  return IRCtx_.create<ir::IRConstantStruct>(Ty, std::move(Vals));
}

ir::IRConstantStruct*
IRConstantEmitter::EmitConstStruct(ir::IRStructType* Ty,
                                    ir::ArrayRef<ir::IRConstant*> Vals) {
  ir::SmallVector<ir::IRConstant*, 16> SV(Vals.begin(), Vals.end());
  return EmitConstStruct(Ty, std::move(SV));
}

ir::IRConstantArray*
IRConstantEmitter::EmitConstArray(ir::IRArrayType* Ty,
                                   ir::SmallVector<ir::IRConstant*, 16> Vals) {
  return IRCtx_.create<ir::IRConstantArray>(Ty, std::move(Vals));
}

ir::IRConstantArray*
IRConstantEmitter::EmitConstArray(ir::IRArrayType* Ty,
                                   ir::ArrayRef<ir::IRConstant*> Vals) {
  ir::SmallVector<ir::IRConstant*, 16> SV(Vals.begin(), Vals.end());
  return EmitConstArray(Ty, std::move(SV));
}
```

**验证来源**：
- `IRConstantStruct(IRStructType*, SmallVector<IRConstant*, 16>)` — `IRConstant.h:89-90`
- `IRConstantArray(IRArrayType*, SmallVector<IRConstant*, 16>)` — `IRConstant.h:100-101`
- `IRStructType::getNumFields()` / `getFieldType(i)` — `IRType.h:156-157`
- `SmallVector` 支持 `{begin, end}` 迭代器构造 — `ADT/SmallVector.h`

---

## 六、集成方式

### 6.1 独立使用

```cpp
ir::IRContext IRCtx;
ir::IRTypeContext& TC = IRCtx.getTypeContext();
IRConstantEmitter ConstEmitter(IRCtx, TC);

auto* CI = ConstEmitter.EmitConstInt(42, 32);
auto* CF = ConstEmitter.EmitConstFloat(3.14);
auto* CN = ConstEmitter.EmitConstNull(TC.getPointerType(TC.getInt8Ty()));
```

### 6.2 从 ASTToIRConverter 中使用

IREmitExpr 已直接通过 `Converter_.getIRContext().create<T>(...)` 创建常量。
IRConstantEmitter 可作为更高级的封装，也可由 IREmitExpr 持有一个实例：

```cpp
// 可选集成：在 IREmitExpr 中
class IREmitExpr {
  // ... 现有成员 ...
  std::unique_ptr<IRConstantEmitter> ConstEmitter_;
public:
  IRConstantEmitter& getConstEmitter() { return *ConstEmitter_; }
};
```

此集成模式不在 B.9 范围内，留待后续优化。

---

## 七、CMakeLists.txt 修改

```cmake
# src/Frontend/CMakeLists.txt
# 在 blocktype_frontend 源文件列表中添加：
  IRConstantEmitter.cpp
```

```cmake
# tests/unit/Frontend/CMakeLists.txt
# 在测试源文件列表中添加：
  IRConstantEmitterTest.cpp
```

---

## 八、测试方案（8 个 GoogleTest 用例）

```cpp
// IRConstantEmitterTest.cpp
#include <gtest/gtest.h>
#include "blocktype/Frontend/IRConstantEmitter.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRConstant.h"

using namespace blocktype;
using namespace blocktype::ir;
using namespace blocktype::frontend;

class IRConstantEmitterTest : public ::testing::Test {
protected:
  IRContext IRCtx;
  IRTypeContext& TC;
  IRConstantEmitter Emitter;

  IRConstantEmitterTest()
    : TC(IRCtx.getTypeContext()), Emitter(IRCtx, TC) {}
};

// V1: 整数常量（APInt 版本）
TEST_F(IRConstantEmitterTest, EmitConstIntAPInt) {
  APInt Val(32, 42);
  auto* CI = Emitter.EmitConstInt(Val);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getZExtValue(), 42u);
  EXPECT_TRUE(CI->getType()->isInteger());
}

// V2: 整数常量（便捷版本）
TEST_F(IRConstantEmitterTest, EmitConstIntConvenience) {
  auto* CI = Emitter.EmitConstInt(255, 8);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getZExtValue(), 255u);

  auto* CI64 = Emitter.EmitConstInt(1000, 64);
  ASSERT_NE(CI64, nullptr);
  EXPECT_EQ(CI64->getZExtValue(), 1000u);
}

// V3: 有符号整数常量
TEST_F(IRConstantEmitterTest, EmitConstIntSigned) {
  auto* CI = Emitter.EmitConstInt(static_cast<uint64_t>(-1), 32, true);
  ASSERT_NE(CI, nullptr);
  EXPECT_EQ(CI->getSExtValue(), -1);
}

// V4: 浮点常量
TEST_F(IRConstantEmitterTest, EmitConstFloat) {
  APFloat FVal(APFloat::Semantics::Double, 3.14);
  auto* CF = Emitter.EmitConstFloat(FVal);
  ASSERT_NE(CF, nullptr);
  EXPECT_TRUE(CF->getType()->isFloat());
  EXPECT_FALSE(CF->isNaN());
  EXPECT_FALSE(CF->isZero());
}

// V5: 浮点常量（便捷版本）
TEST_F(IRConstantEmitterTest, EmitConstFloatConvenience) {
  auto* CF32 = Emitter.EmitConstFloat(1.5f, 32);
  ASSERT_NE(CF32, nullptr);
  EXPECT_TRUE(CF32->getType()->isFloat());

  auto* CF64 = Emitter.EmitConstFloat(2.718, 64);
  ASSERT_NE(CF64, nullptr);
}

// V6: 空值 / 未定义值 / 零聚合
TEST_F(IRConstantEmitterTest, EmitSpecialConstants) {
  auto* Null = Emitter.EmitConstNull(TC.getPointerType(TC.getInt8Ty()));
  ASSERT_NE(Null, nullptr);

  auto* Undef = Emitter.EmitConstUndef(TC.getInt32Ty());
  ASSERT_NE(Undef, nullptr);

  auto* ZeroAgg = Emitter.EmitConstAggregateZero(TC.getInt32Ty());
  ASSERT_NE(ZeroAgg, nullptr);
}

// V7: 结构体常量
TEST_F(IRConstantEmitterTest, EmitConstStruct) {
  SmallVector<IRType*, 16> Fields = {TC.getInt32Ty(), TC.getInt64Ty()};
  auto* STy = TC.getStructType("TestStruct", Fields);
  ASSERT_NE(STy, nullptr);

  auto* F1 = Emitter.EmitConstInt(10, 32);
  auto* F2 = Emitter.EmitConstInt(20, 64);
  SmallVector<IRConstant*, 16> Vals = {F1, F2};
  auto* CS = Emitter.EmitConstStruct(STy, Vals);
  ASSERT_NE(CS, nullptr);
  EXPECT_EQ(CS->getElements().size(), 2u);
}

// V8: 数组常量
TEST_F(IRConstantEmitterTest, EmitConstArray) {
  auto* ATy = TC.getArrayType(TC.getInt32Ty(), 3);
  ASSERT_NE(ATy, nullptr);

  auto* E0 = Emitter.EmitConstInt(1, 32);
  auto* E1 = Emitter.EmitConstInt(2, 32);
  auto* E2 = Emitter.EmitConstInt(3, 32);
  SmallVector<IRConstant*, 16> Vals = {E0, E1, E2};
  auto* CA = Emitter.EmitConstArray(ATy, Vals);
  ASSERT_NE(CA, nullptr);
  EXPECT_EQ(CA->getElements().size(), 3u);
}
```

---

## 九、验收标准

| 版本 | 验收项 | 验证方式 |
|------|--------|---------|
| V1 | 整数常量（APInt + 便捷版本 + 有符号） | `EmitConstInt(42, 32)` → `getZExtValue() == 42` |
| V2 | 浮点常量（APFloat + 便捷版本） | `EmitConstFloat(3.14)` → `!isNaN() && !isZero()` |
| V3 | 结构体/数组聚合常量 | 构造 → 验证 `getElements().size()` |
| V4 | 特殊常量（null/undef/aggzero） | 构造 → `!= nullptr` |
| V5 | 编译通过 + 全部 8 个测试通过 | `ctest -R IRConstantEmitter` |

---

## 十、实现风险与注意事项

### 10.1 APFloat::getSemantics() 可用性

`APFloat` 的 `Sem` 成员是 `private`。需确认是否有公开的 `getSemantics()` accessor：

```cpp
// 如果 APFloat 没有 getSemantics()，需要以下替代方案之一：
// 方案 A：添加 getter（推荐，一行代码）
// 方案 B：让 EmitConstFloat 接受 IRFloatType* 参数
// 方案 C：接受 Semantics 参数
```

**检查方法**：`grep getSemantics include/blocktype/IR/ADT/APFloat.h`

### 10.2 IRTypeContext 与 IRContext 的关系

`IRContext` 内部持有 `IRTypeContext`（`IRContext.h:31`），可通过 `getTypeContext()` 获取。
构造函数接收两者是为了灵活性（独立使用时只需传 `IRCtx` + `IRCtx.getTypeContext()`）。

### 10.3 EmitConstUndef 的缓存行为

`IRConstantUndef::get()` 内部使用 `IRContext::UndefCache`（`DenseMap<IRType*, IRConstantUndef*>`），
对同一类型多次调用返回同一实例。这是正确行为。

### 10.4 SmallVector 的初始化列表构造

`SmallVector` 可能不支持 `std::initializer_list` 构造。测试中应使用显式构造。
如果 `SmallVector` 有 `{a, b, c}` 初始化支持则可直接使用，否则使用 `push_back`。

---

## 十一、Git 提交

```bash
git add include/blocktype/Frontend/IRConstantEmitter.h \
        src/Frontend/IRConstantEmitter.cpp \
        tests/unit/Frontend/IRConstantEmitterTest.cpp
git commit -m "feat(B): 完成 Task B.9：IRConstantEmitter — 后端无关常量发射器"
```
