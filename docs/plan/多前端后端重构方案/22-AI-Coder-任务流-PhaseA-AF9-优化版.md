# Task A-F9 优化版：IRConversionResult + IRVerificationResult（错误恢复机制）

> 文档版本：v1.0 | 由 planner 产出

---

## 一、任务概述

| 项目 | 内容 |
|------|------|
| 任务编号 | A-F9 |
| 任务名称 | IRConversionResult + IRVerificationResult（错误恢复机制） |
| 依赖 | A.4（IRModule）— ✅ 已存在 |
| 对现有代码的影响 | 仅 CMakeLists 追加条目 |

**产出文件**

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRConversionResult.h` |
| 新增 | `src/IR/IRConversionResult.cpp` |
| 新增 | `tests/unit/IR/IRConversionResultTest.cpp` |
| 修改 | `src/IR/CMakeLists.txt`（+1 行） |
| 修改 | `tests/unit/IR/CMakeLists.txt`（+1 行） |

---

## 二、设计分析

### 2.1 项目适配要点

| 要点 | 适配方案 |
|------|----------|
| `SmallVector` 在 `blocktype::ir` 命名空间 | `IRVerificationResult` 在 `ir` 命名空间内，直接使用 `SmallVector<std::string, 8>` |
| `IRModule` 构造需要 `IRTypeContext&` | 头文件前向声明 `IRModule`，测试中通过 `IRContext::getTypeContext()` 获取 |
| 头文件只需 ADT + 前向声明 | `#include "blocktype/IR/ADT.h"`，不 include `IRModule.h` |
| `IRConversionResult` 持有 `unique_ptr<IRModule>` | 头文件前向声明，.cpp 中完整 include |

### 2.2 错误恢复策略表

| 阶段 | 错误类型 | 恢复策略 |
|------|---------|---------|
| AST → IR 转换 | 类型映射失败 | 发出 Diag，用 `IROpaqueType` 占位，继续转换 |
| AST → IR 转换 | 语句转换失败 | 发出 Diag，跳过该语句，继续下一语句 |
| AST → IR 转换 | 表达式转换失败 | 发出 Diag，用 `IRConstantUndef` 占位，继续 |
| IR 验证 | 类型完整性 | 收集所有违规，一次性报告 |
| IR 验证 | SSA 性质 | 收集所有违规，一次性报告 |
| IR → LLVM 转换 | 不支持的 IR 特性 | 发出 Diag，中止该函数转换，继续下一函数 |
| IR → LLVM 转换 | 后端能力不足 | 检查 `BackendCapability`，提前报告不支持的特性 |

**约束**：IR 转换的错误都是硬错误，直接通过 DiagnosticsEngine 报告。IR 转换层不需要 SFINAE 抑制。

### 2.3 命名空间

spec 使用 `blocktype::ir`（直接在 `ir` 下）。合理：这是 IR 层核心类型。

---

## 三、完整头文件 — `include/blocktype/IR/IRConversionResult.h`

```cpp
#ifndef BLOCKTYPE_IR_IRCONVERSIONRESULT_H
#define BLOCKTYPE_IR_IRCONVERSIONRESULT_H

#include <memory>
#include <string>

#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace ir {

class IRModule;

// ============================================================
// IRConversionResult — IR 转换结果
// ============================================================

/// 封装前端到 IR 转换的结果。
/// 有效转换持有 IRModule；无效转换记录错误数量。
class IRConversionResult {
  std::unique_ptr<IRModule> Module_;
  bool Invalid_ = false;
  unsigned NumErrors_ = 0;

public:
  IRConversionResult() = default;

  explicit IRConversionResult(std::unique_ptr<IRModule> M)
    : Module_(std::move(M)) {}

  static IRConversionResult getInvalid(unsigned Errors = 1) {
    IRConversionResult R;
    R.Invalid_ = true;
    R.NumErrors_ = Errors;
    return R;
  }

  bool isInvalid() const { return Invalid_; }
  bool isUsable() const { return Module_ != nullptr && !Invalid_; }
  std::unique_ptr<IRModule> takeModule() { return std::move(Module_); }
  unsigned getNumErrors() const { return NumErrors_; }
};

// ============================================================
// IRVerificationResult — IR 验证结果
// ============================================================

/// 封装 IR 验证结果。支持收集多条违规信息，一次性报告。
class IRVerificationResult {
  bool IsValid_;
  SmallVector<std::string, 8> Violations_;

public:
  explicit IRVerificationResult(bool Valid) : IsValid_(Valid) {}

  void addViolation(const std::string& Msg) {
    Violations_.push_back(Msg);
    IsValid_ = false;
  }

  bool isValid() const { return IsValid_; }
  const SmallVector<std::string, 8>& getViolations() const { return Violations_; }
  size_t getNumViolations() const { return Violations_.size(); }
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCONVERSIONRESULT_H
```

---

## 四、完整实现文件 — `src/IR/IRConversionResult.cpp`

```cpp
#include "blocktype/IR/IRConversionResult.h"

// IRConversionResult 和 IRVerificationResult 的实现全部在头文件中（inline）。
// 此 .cpp 文件存在是为了：
// 1. 确保头文件的自包含性（编译验证）
// 2. 为后续扩展预留（如添加 toString()、日志等方法）

namespace blocktype {
namespace ir {

// 预留后续扩展：
// - IRConversionResult::toString() — 生成人类可读的结果描述
// - IRVerificationResult::toJSON() — 生成结构化的验证报告

} // namespace ir
} // namespace blocktype
```

---

## 五、完整测试文件 — `tests/unit/IR/IRConversionResultTest.cpp`

```cpp
#include <gtest/gtest.h>
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRModule.h"

using namespace blocktype;
using namespace blocktype::ir;

// V1: IRConversionResult 有效
TEST(IRConversionResultTest, ValidResult) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("test", Ctx.getTypeContext());
  IRConversionResult Result(std::move(Mod));
  EXPECT_FALSE(Result.isInvalid());
  EXPECT_TRUE(Result.isUsable());
  EXPECT_EQ(Result.getNumErrors(), 0u);
}

// V2: IRConversionResult 无效
TEST(IRConversionResultTest, InvalidResult) {
  auto InvalidResult = IRConversionResult::getInvalid(3);
  EXPECT_TRUE(InvalidResult.isInvalid());
  EXPECT_FALSE(InvalidResult.isUsable());
  EXPECT_EQ(InvalidResult.getNumErrors(), 3u);
}

// V3: IRVerificationResult 收集违规
TEST(IRConversionResultTest, VerificationViolations) {
  IRVerificationResult VR(true);
  EXPECT_TRUE(VR.isValid());
  VR.addViolation("Missing terminator in BB entry");
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getViolations().size(), 1u);
}

// V4: takeModule 转移所有权
TEST(IRConversionResultTest, TakeModule) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("owned", Ctx.getTypeContext());
  IRConversionResult Result(std::move(Mod));
  EXPECT_TRUE(Result.isUsable());
  auto Taken = Result.takeModule();
  ASSERT_NE(Taken, nullptr);
  EXPECT_EQ(Taken->getName(), "owned");
  EXPECT_FALSE(Result.isUsable());
}

// V5: 默认构造
TEST(IRConversionResultTest, DefaultConstruct) {
  IRConversionResult R;
  EXPECT_FALSE(R.isInvalid());
  EXPECT_FALSE(R.isUsable());
  EXPECT_EQ(R.getNumErrors(), 0u);
}

// V6: getInvalid 默认错误数
TEST(IRConversionResultTest, GetInvalidDefault) {
  auto R = IRConversionResult::getInvalid();
  EXPECT_TRUE(R.isInvalid());
  EXPECT_EQ(R.getNumErrors(), 1u);
}

// V7: 多条违规
TEST(IRConversionResultTest, MultipleViolations) {
  IRVerificationResult VR(true);
  VR.addViolation("Error 1");
  VR.addViolation("Error 2");
  VR.addViolation("Error 3");
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getNumViolations(), 3u);
  EXPECT_EQ(VR.getViolations()[0], "Error 1");
  EXPECT_EQ(VR.getViolations()[2], "Error 3");
}

// V8: 初始无效
TEST(IRConversionResultTest, InitiallyInvalid) {
  IRVerificationResult VR(false);
  EXPECT_FALSE(VR.isValid());
  EXPECT_EQ(VR.getNumViolations(), 0u);
}

// V9: 有效模块含 TargetTriple
TEST(IRConversionResultTest, ValidModuleWithName) {
  IRContext Ctx;
  auto Mod = std::make_unique<IRModule>("my_module", Ctx.getTypeContext(),
                                         "x86_64-linux-gnu");
  IRConversionResult Result(std::move(Mod));
  EXPECT_TRUE(Result.isUsable());
  auto M = Result.takeModule();
  ASSERT_NE(M, nullptr);
  EXPECT_EQ(M->getName(), "my_module");
  EXPECT_EQ(M->getTargetTriple(), "x86_64-linux-gnu");
}

// V10: 违规内容正确
TEST(IRConversionResultTest, ViolationContent) {
  IRVerificationResult VR(true);
  VR.addViolation("Type mismatch: expected i32, got i64");
  const auto& V = VR.getViolations();
  ASSERT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0], "Type mismatch: expected i32, got i64");
}
```

---

## 六、CMakeLists.txt 修改

### `src/IR/CMakeLists.txt` — 追加：
```cmake
  IRConversionResult.cpp
```

### `tests/unit/IR/CMakeLists.txt` — 追加：
```cmake
  IRConversionResultTest.cpp
```

---

## 七、验收标准映射

| 编号 | 验收内容 | 对应测试 |
|------|----------|----------|
| V1 | IRConversionResult 有效 | `ValidResult` |
| V2 | IRConversionResult 无效 | `InvalidResult` |
| V3 | IRVerificationResult 收集违规 | `VerificationViolations` |
| V4 | takeModule 转移所有权 | `TakeModule` |
| V5 | 默认构造 | `DefaultConstruct` |
| V6 | getInvalid 默认错误数 | `GetInvalidDefault` |
| V7 | 多条违规 | `MultipleViolations` |
| V8 | 初始无效 | `InitiallyInvalid` |
| V9 | 有效模块含 TargetTriple | `ValidModuleWithName` |
| V10 | 违规内容 | `ViolationContent` |

---

## 八、sizeof 影响评估

| 类型 | 大小 | 说明 |
|------|------|------|
| IRConversionResult | ~24 bytes | unique_ptr(8) + bool(1) + unsigned(4) + padding |
| IRVerificationResult | ~200 bytes | bool + SmallVector\<string,8\> 内联存储 |

---

## 九、风险与注意事项

1. **IRModule 前向声明**：头文件只前向声明，测试 .cpp include `IRModule.h`
2. **takeModule() 后状态**：`Module_` 变为 nullptr，`isUsable()` 返回 false
3. **错误恢复策略是文档性质**：A-F9 仅定义结果封装类型，恢复逻辑在集成阶段实现
4. **红线 Checklist 全部通过**：纯新增、无耦合、不退化

---

## 十、实现步骤

| 步骤 | 操作 |
|------|------|
| 1 | 创建 `include/blocktype/IR/IRConversionResult.h` |
| 2 | 创建 `src/IR/IRConversionResult.cpp` |
| 3 | 修改 `src/IR/CMakeLists.txt` |
| 4 | 修改 `tests/unit/IR/CMakeLists.txt` |
| 5 | 创建 `tests/unit/IR/IRConversionResultTest.cpp` |
| 6 | 编译 + 测试 + 全量 ctest |
| 7 | Git commit |
