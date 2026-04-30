# Task B-F4：IREquivalenceChecker 接口定义（优化版 - 修订）

## 修订记录

| 日期 | 修订内容 |
|------|----------|
| 2026-04-30 | 修正 `getIntegerType` → `getIntType` |

---

## 1. 代码库验证摘要

| 规格中的类型 | 实际代码库 | 修正 |
|-------------|-----------|------|
| `ir::IRModule` | `ir::IRModule`（`IR/IRModule.h`） | ✅ 直接使用 |
| `ir::IRFunction` | `ir::IRFunction`（`IR/IRFunction.h`） | ✅ 直接使用 |
| `ir::IRType` | `ir::IRType`（`IR/IRType.h`） | ✅ 直接使用 |
| `ir::SmallVector` | `ir::SmallVector`（`IR/ADT/SmallVector.h`） | ✅ 直接使用 |
| `IRTypeContext::getIntegerType` | `IRTypeContext::getIntType` | ⚠️ 已修正 |

**无冲突，纯接口定义，最简单的任务。**

---

## 2. 完整类定义

### `include/blocktype/IR/IREquivalenceChecker.h`

```cpp
#ifndef BLOCKTYPE_IR_IREQUIVALENCECHECKER_H
#define BLOCKTYPE_IR_IREQUIVALENCECHECKER_H

#include <string>
#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;
class IRFunction;
class IRType;

// ============================================================
// IREquivalenceChecker — IR 等价性检查（接口定义）
// ============================================================
//
// 纯接口，实现留待 Phase E (E-F3)。

class IREquivalenceChecker {
public:
  struct EquivalenceResult {
    bool IsEquivalent = false;
    ir::SmallVector<std::string, 8> Differences;
  };

  /// 检查两个 IRModule 是否等价
  static EquivalenceResult check(const IRModule& A, const IRModule& B);

  /// 检查两个 IRFunction 是否结构等价
  static bool isStructurallyEquivalent(const IRFunction& A, const IRFunction& B);

  /// 检查两个 IRType 是否等价
  static bool isTypeEquivalent(const IRType* A, const IRType* B);
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IREQUIVALENCECHECKER_H
```

### `src/IR/IREquivalenceChecker.cpp`

```cpp
#include "blocktype/IR/IREquivalenceChecker.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRType.h"

namespace blocktype {
namespace ir {

IREquivalenceChecker::EquivalenceResult
IREquivalenceChecker::check(const IRModule& A, const IRModule& B) {
  EquivalenceResult Result;
  // Stub: 比较函数数量
  if (A.getNumFunctions() != B.getNumFunctions()) {
    Result.IsEquivalent = false;
    Result.Differences.push_back("Different number of functions");
    return Result;
  }
  // Stub: 简单名称对比
  auto& FA = A.getFunctions();
  auto& FB = B.getFunctions();
  auto ItA = FA.begin();
  auto ItB = FB.begin();
  for (; ItA != FA.end() && ItB != FB.end(); ++ItA, ++ItB) {
    if ((*ItA)->getName() != (*ItB)->getName()) {
      Result.Differences.push_back(
        "Function name mismatch: " + (*ItA)->getName().str() +
        " vs " + (*ItB)->getName().str());
    }
  }
  Result.IsEquivalent = Result.Differences.empty();
  return Result;
}

bool IREquivalenceChecker::isStructurallyEquivalent(
    const IRFunction& A, const IRFunction& B) {
  // Stub: 比较名称和参数数量
  if (A.getName() != B.getName()) return false;
  if (A.getNumArgs() != B.getNumArgs()) return false;
  if (A.getNumBasicBlocks() != B.getNumBasicBlocks()) return false;
  return true;
}

bool IREquivalenceChecker::isTypeEquivalent(const IRType* A, const IRType* B) {
  if (!A && !B) return true;
  if (!A || !B) return false;
  return A->equals(B);
}

} // namespace ir
} // namespace blocktype
```

---

## 3. 产出文件列表

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IREquivalenceChecker.h` |
| 新增 | `src/IR/IREquivalenceChecker.cpp` |
| 新增 | `tests/unit/IR/IREquivalenceCheckerTest.cpp` |

---

## 4. CMakeLists.txt 修改

### `src/IR/CMakeLists.txt`

在 `add_library(blocktype-ir ...)` 的源文件列表中添加：

```
IREquivalenceChecker.cpp
```

### `tests/unit/IR/CMakeLists.txt`

在 `add_executable(blocktype-ir-test ...)` 的源文件列表中添加：

```
IREquivalenceCheckerTest.cpp
```

---

## 5. GTest 测试用例

### `tests/unit/IR/IREquivalenceCheckerTest.cpp`

```cpp
#include <gtest/gtest.h>
#include <memory>
#include "blocktype/IR/IREquivalenceChecker.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRTypeContext.h"

using namespace blocktype::ir;

namespace {

/// 辅助：创建包含一个空函数的模块
std::unique_ptr<IRModule> makeModuleWithFunc(
    IRTypeContext& Ctx, StringRef ModName, StringRef FuncName) {
  auto Mod = std::make_unique<IRModule>(ModName, Ctx);
  auto* VoidTy = Ctx.getVoidType();
  auto* FnTy = Ctx.getFunctionType(VoidTy, {});
  auto Fn = std::make_unique<IRFunction>(Mod.get(), FuncName, FnTy);
  Mod->addFunction(std::move(Fn));
  return Mod;
}

} // anonymous namespace

TEST(IREquivalenceCheckerTest, SameModuleIsEquivalent) {
  IRTypeContext Ctx;
  auto Mod = makeModuleWithFunc(Ctx, "test", "main");
  auto Result = IREquivalenceChecker::check(*Mod, *Mod);
  EXPECT_TRUE(Result.IsEquivalent);
  EXPECT_TRUE(Result.Differences.empty());
}

TEST(IREquivalenceCheckerTest, DifferentFuncCountNotEquivalent) {
  IRTypeContext Ctx;
  auto ModA = makeModuleWithFunc(Ctx, "a", "foo");
  auto ModB = std::make_unique<IRModule>("b", Ctx);
  auto Result = IREquivalenceChecker::check(*ModA, *ModB);
  EXPECT_FALSE(Result.IsEquivalent);
  EXPECT_FALSE(Result.Differences.empty());
}

TEST(IREquivalenceCheckerTest, TypeEquivalent) {
  IRTypeContext Ctx;
  auto* Int32_A = Ctx.getIntType(32);  // ✅ 修正：使用 getIntType
  auto* Int32_B = Ctx.getIntType(32);  // ✅ 修正：使用 getIntType
  EXPECT_TRUE(IREquivalenceChecker::isTypeEquivalent(Int32_A, Int32_B));
}

TEST(IREquivalenceCheckerTest, TypeNotEquivalent) {
  IRTypeContext Ctx;
  auto* Int32 = Ctx.getIntType(32);   // ✅ 修正：使用 getIntType
  auto* Int64 = Ctx.getIntType(64);   // ✅ 修正：使用 getIntType
  EXPECT_FALSE(IREquivalenceChecker::isTypeEquivalent(Int32, Int64));
}

TEST(IREquivalenceCheckerTest, NullTypeEquivalent) {
  EXPECT_TRUE(IREquivalenceChecker::isTypeEquivalent(nullptr, nullptr));
  EXPECT_FALSE(IREquivalenceChecker::isTypeEquivalent(nullptr,
      reinterpret_cast<IRType*>(1)));
}

TEST(IREquivalenceCheckerTest, StructurallyEquivalent) {
  IRTypeContext Ctx;
  auto ModA = makeModuleWithFunc(Ctx, "a", "foo");
  auto ModB = makeModuleWithFunc(Ctx, "b", "foo");
  EXPECT_TRUE(IREquivalenceChecker::isStructurallyEquivalent(
      *ModA->getFunction("foo"), *ModB->getFunction("foo")));
}
```

---

## 6. 验收标准

1. `ir::IREquivalenceChecker::check()` 返回 `EquivalenceResult`
2. `ir::IREquivalenceChecker::isStructurallyEquivalent()` 比较函数结构
3. `ir::IREquivalenceChecker::isTypeEquivalent()` 委托给 `IRType::equals()`
4. 仅定义接口 + stub 实现，完整实现留待 Phase E
5. 所有 GTest 测试通过
6. 不修改任何现有文件（仅新增）
