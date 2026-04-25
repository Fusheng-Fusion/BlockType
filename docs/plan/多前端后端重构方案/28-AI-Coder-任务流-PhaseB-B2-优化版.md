# Task B.2 优化版：FrontendRegistry + 自动选择

> **状态**: planner 优化完成，待 team-lead 审阅
> **原始规格**: `12-AI-Coder-任务流-PhaseB.md` 第 269~348 行
> **产出文件**: 2 个新增文件 + 1 个新增测试文件 + CMakeLists 修改
> **依赖**: B.1（FrontendBase + FrontendCompileOptions + FrontendFactory）

---

## 红线 Checklist（dev-tester 执行前逐条确认）

| # | 红线 | 验证方式 |
|---|------|----------|
| 1 | 架构优先 | FrontendRegistry 管理 FrontendFactory，通过抽象接口创建前端，不依赖具体前端实现 |
| 2 | 多前端多后端自由组合 | registerFrontend 可注册任意前端，autoSelect 按扩展名自动路由 |
| 3 | 渐进式改造 | 仅新增文件 + CMakeLists 改一行，不修改现有代码 |
| 4 | 现有功能不退化 | 不触碰 CompilerInstance/CompilerInvocation 等现有代码 |
| 5 | 接口抽象优先 | Registry 只操作 FrontendBase 抽象接口和 FrontendFactory 类型别名 |
| 6 | IR 中间层解耦 | Registry 不涉及 IR 层，仅管理前端实例的创建 |

---

## 规格偏差修正记录

| 原始规格写法 | 优化版修正 | 原因 |
|-------------|-----------|------|
| `llvm::StringMap<FrontendFactory>` | `ir::StringMap<FrontendFactory>` | 项目使用 `blocktype::ir::StringMap`，非 `llvm::StringMap` |
| `llvm::StringMap<std::string>` | `ir::StringMap<std::string>` | 同上 |
| `const FrontendOptions& Opts` | `const FrontendCompileOptions& Opts` | B.1 决策：新结构体命名为 `FrontendCompileOptions` |
| `unique_ptr<FrontendBase>` | `std::unique_ptr<FrontendBase>` | C++ 标准写法，项目无 `unique_ptr` 别名 |
| `SmallVector<StringRef, 4>` | `ir::SmallVector<std::string, 4>` | `ir::StringMap::Entry` 中 Key 是 `std::string`；`ir::StringRef` 不持有数据所有权，返回 `std::string` 更安全 |
| `StringRef` 作为参数 | `ir::StringRef` 或 `std::string_view` | 前端命名空间中使用 `ir::StringRef` 需显式限定或 using 声明 |

---

## Part 1: 头文件 FrontendRegistry.h（完整代码）

**文件路径**: `include/blocktype/Frontend/FrontendRegistry.h`

```cpp
//===--- FrontendRegistry.h - Frontend Registry -------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the FrontendRegistry class, a global singleton that
// manages frontend registration and automatic frontend selection based
// on file extensions.
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H
#define BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H

#include <cassert>
#include <memory>
#include <string>

#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/FrontendBase.h"
#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace frontend {

/// FrontendRegistry - Global singleton for frontend registration and creation.
///
/// Manages a registry of FrontendFactory functions keyed by frontend name,
/// plus an extension-to-frontend-name mapping for automatic frontend selection.
///
/// Thread safety: Registration is expected to occur during initialization
/// (single-threaded). Lookup/creation may be called from any thread after
/// registration is complete (read-only access to internal maps).
class FrontendRegistry {
  /// Maps frontend name (e.g., "cpp") → factory function.
  ir::StringMap<FrontendFactory> Registry_;

  /// Maps file extension (e.g., ".cpp") → frontend name.
  ir::StringMap<std::string> ExtToName_;

  /// Private constructor for singleton pattern.
  FrontendRegistry() = default;

public:
  /// Get the global FrontendRegistry singleton instance.
  static FrontendRegistry& instance();

  // Non-copyable, non-movable.
  FrontendRegistry(const FrontendRegistry&) = delete;
  FrontendRegistry& operator=(const FrontendRegistry&) = delete;
  FrontendRegistry(FrontendRegistry&&) = delete;
  FrontendRegistry& operator=(FrontendRegistry&&) = delete;

  /// Register a frontend factory under the given name.
  ///
  /// \param Name     Frontend name (e.g., "cpp", "bt").
  /// \param Factory  Factory function to create instances of this frontend.
  ///
  /// Precondition: No frontend with the same name is already registered.
  /// Asserts on duplicate registration.
  void registerFrontend(ir::StringRef Name, FrontendFactory Factory);

  /// Create a frontend instance by name.
  ///
  /// \param Name   Registered frontend name.
  /// \param Opts   Compile options for the frontend.
  /// \param Diags  Diagnostics engine for error reporting.
  /// \returns Owning pointer to the created frontend, or nullptr if not found.
  std::unique_ptr<FrontendBase> create(
    ir::StringRef Name,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags);

  /// Automatically select and create a frontend based on file extension.
  ///
  /// Extracts the file extension from Filename, looks up the extension
  /// mapping, and creates the corresponding frontend.
  ///
  /// \param Filename  Source file path (used to extract extension).
  /// \param Opts      Compile options for the frontend.
  /// \param Diags     Diagnostics engine for error reporting.
  /// \returns Owning pointer to the created frontend, or nullptr if no
  ///          matching extension mapping is found.
  std::unique_ptr<FrontendBase> autoSelect(
    ir::StringRef Filename,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags);

  /// Add a file extension to frontend name mapping.
  ///
  /// \param Ext           File extension including dot (e.g., ".cpp").
  /// \param FrontendName  Name of the registered frontend.
  void addExtensionMapping(ir::StringRef Ext, ir::StringRef FrontendName);

  /// Check if a frontend with the given name is registered.
  bool hasFrontend(ir::StringRef Name) const;

  /// Get all registered frontend names.
  ir::SmallVector<std::string, 4> getRegisteredNames() const;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H
```

---

## Part 2: 实现文件 FrontendRegistry.cpp（完整代码）

**文件路径**: `src/Frontend/FrontendRegistry.cpp`

```cpp
//===--- FrontendRegistry.cpp - Frontend Registry -----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Frontend/FrontendRegistry.h"

#include <cassert>

namespace blocktype {
namespace frontend {

FrontendRegistry& FrontendRegistry::instance() {
  static FrontendRegistry Inst;
  return Inst;
}

void FrontendRegistry::registerFrontend(ir::StringRef Name,
                                         FrontendFactory Factory) {
  std::string Key = Name.str();
  assert(!Registry_.contains(Key) &&
         "FrontendRegistry: duplicate frontend registration");
  Registry_.insert({std::move(Key), std::move(Factory)});
}

std::unique_ptr<FrontendBase> FrontendRegistry::create(
    ir::StringRef Name,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags) {
  auto It = Registry_.find(Name.str());
  if (It == Registry_.end())
    return nullptr;
  return It->Value(Opts, Diags);
}

std::unique_ptr<FrontendBase> FrontendRegistry::autoSelect(
    ir::StringRef Filename,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags) {
  // Extract file extension: find the last '.' in the filename.
  size_t DotPos = Filename.rfind('.');
  if (DotPos == ir::StringRef::npos)
    return nullptr;

  std::string Ext = Filename.slice(DotPos).str();
  auto It = ExtToName_.find(Ext);
  if (It == ExtToName_.end())
    return nullptr;

  return create(It->Value, Opts, Diags);
}

void FrontendRegistry::addExtensionMapping(ir::StringRef Ext,
                                             ir::StringRef FrontendName) {
  ExtToName_.insert({Ext.str(), FrontendName.str()});
}

bool FrontendRegistry::hasFrontend(ir::StringRef Name) const {
  return Registry_.contains(Name.str());
}

ir::SmallVector<std::string, 4> FrontendRegistry::getRegisteredNames() const {
  ir::SmallVector<std::string, 4> Names;
  for (const auto& Entry : Registry_)
    Names.push_back(Entry.Key);
  return Names;
}

} // namespace frontend
} // namespace blocktype
```

---

## Part 3: CMakeLists 修改

### 3.1 src/Frontend/CMakeLists.txt

**修改内容**：在 `add_library` 源文件列表中新增 `FrontendRegistry.cpp`。

**修改前**（B.1 完成后的状态）：
```cmake
add_library(blocktypeFrontend
  CompilerInvocation.cpp
  CompilerInstance.cpp
  FrontendBase.cpp
)
```

**修改后**：
```cmake
add_library(blocktypeFrontend
  CompilerInvocation.cpp
  CompilerInstance.cpp
  FrontendBase.cpp
  FrontendRegistry.cpp
)
```

> **注意**：仅添加 `FrontendRegistry.cpp` 一行，其余内容不变。

---

## Part 4: 测试文件（完整代码）

**文件路径**: `tests/Frontend/test_frontend_registry.cpp`

```cpp
//===--- test_frontend_registry.cpp - FrontendRegistry tests ---*- C++ -*-===//

#include <cassert>
#include <memory>
#include <string>

#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/FrontendBase.h"
#include "blocktype/Frontend/FrontendCompileOptions.h"
#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/TargetLayout.h"

using namespace blocktype;
using namespace blocktype::frontend;
using namespace blocktype::ir;

// ============================================================
// Test helper: A concrete frontend for registry testing
// ============================================================

class MockCppFrontend : public FrontendBase {
public:
  using FrontendBase::FrontendBase;

  ir::StringRef getName() const override { return "cpp"; }
  ir::StringRef getLanguage() const override { return "c++"; }

  std::unique_ptr<ir::IRModule> compile(
      ir::StringRef Filename,
      ir::IRTypeContext& TypeCtx,
      const ir::TargetLayout& Layout) override {
    return std::make_unique<ir::IRModule>(Filename, TypeCtx,
                                           Opts_.TargetTriple);
  }

  bool canHandle(ir::StringRef Filename) const override {
    return Filename.endswith(".cpp") || Filename.endswith(".cc") ||
           Filename.endswith(".cxx") || Filename.endswith(".c");
  }
};

class MockBtFrontend : public FrontendBase {
public:
  using FrontendBase::FrontendBase;

  ir::StringRef getName() const override { return "bt"; }
  ir::StringRef getLanguage() const override { return "blocktype"; }

  std::unique_ptr<ir::IRModule> compile(
      ir::StringRef Filename,
      ir::IRTypeContext& TypeCtx,
      const ir::TargetLayout& Layout) override {
    return std::make_unique<ir::IRModule>(Filename, TypeCtx,
                                           Opts_.TargetTriple);
  }

  bool canHandle(ir::StringRef Filename) const override {
    return Filename.endswith(".bt");
  }
};

// Factory functions
static std::unique_ptr<FrontendBase>
createCppFrontend(const FrontendCompileOptions& Opts,
                  DiagnosticsEngine& Diags) {
  return std::make_unique<MockCppFrontend>(Opts, Diags);
}

static std::unique_ptr<FrontendBase>
createBtFrontend(const FrontendCompileOptions& Opts,
                 DiagnosticsEngine& Diags) {
  return std::make_unique<MockBtFrontend>(Opts, Diags);
}

// ============================================================
// Helper: clear the singleton for test isolation
// ============================================================

/// NOTE: FrontendRegistry is a singleton. For test isolation, we rely
/// on the fact that tests run sequentially and share the same instance.
/// Each test appends to the registry — duplicate registration tests
/// are placed last to avoid polluting the shared state.

// ============================================================
// Test cases
// ============================================================

void test_singleton_identity() {
  auto& A = FrontendRegistry::instance();
  auto& B = FrontendRegistry::instance();
  assert(&A == &B && "instance() must return the same singleton");
}

void test_register_and_create() {
  auto& Reg = FrontendRegistry::instance();
  Reg.registerFrontend("test_cpp", createCppFrontend);

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = Reg.create("test_cpp", Opts, Diags);
  assert(FE != nullptr);
  assert(FE->getName() == "cpp");
  assert(FE->getLanguage() == "c++");
}

void test_auto_select() {
  auto& Reg = FrontendRegistry::instance();
  Reg.registerFrontend("test_bt", createBtFrontend);
  Reg.addExtensionMapping(".bt", "test_bt");

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = Reg.autoSelect("hello.bt", Opts, Diags);
  assert(FE != nullptr);
  assert(FE->getName() == "bt");
}

void test_auto_select_with_path() {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  // Test with full path — should still find the extension.
  auto FE = Reg.autoSelect("/path/to/source/hello.bt", Opts, Diags);
  assert(FE != nullptr);
  assert(FE->getName() == "bt");
}

void test_create_unknown_returns_nullptr() {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.create("nonexistent_frontend", Opts, Diags);
  assert(FE == nullptr);
}

void test_auto_select_unknown_extension_returns_nullptr() {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.autoSelect("unknown.xyz", Opts, Diags);
  assert(FE == nullptr);
}

void test_auto_select_no_extension_returns_nullptr() {
  auto& Reg = FrontendRegistry::instance();

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;

  auto FE = Reg.autoSelect("no_extension_file", Opts, Diags);
  assert(FE == nullptr);
}

void test_has_frontend() {
  auto& Reg = FrontendRegistry::instance();
  assert(Reg.hasFrontend("test_cpp") == true);
  assert(Reg.hasFrontend("test_bt") == true);
  assert(Reg.hasFrontend("nonexistent") == false);
}

void test_get_registered_names() {
  auto& Reg = FrontendRegistry::instance();
  auto Names = Reg.getRegisteredNames();
  // At minimum, we registered "test_cpp" and "test_bt" in previous tests.
  assert(Names.size() >= 2);
  // Verify the names are present.
  bool hasCpp = false, hasBt = false;
  for (const auto& N : Names) {
    if (N == "test_cpp") hasCpp = true;
    if (N == "test_bt") hasBt = true;
  }
  assert(hasCpp && hasBt);
}

void test_extension_mapping() {
  auto& Reg = FrontendRegistry::instance();
  Reg.addExtensionMapping(".cpp", "test_cpp");
  Reg.addExtensionMapping(".cc", "test_cpp");
  Reg.addExtensionMapping(".cxx", "test_cpp");

  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE1 = Reg.autoSelect("test.cpp", Opts, Diags);
  assert(FE1 != nullptr && FE1->getName() == "cpp");

  auto FE2 = Reg.autoSelect("test.cc", Opts, Diags);
  assert(FE2 != nullptr && FE2->getName() == "cpp");

  auto FE3 = Reg.autoSelect("test.cxx", Opts, Diags);
  assert(FE3 != nullptr && FE3->getName() == "cpp");
}

// NOTE: The duplicate registration test will trigger an assert failure.
// Uncomment to verify at your own risk (will abort the test process).
// void test_duplicate_registration_asserts() {
//   auto& Reg = FrontendRegistry::instance();
//   Reg.registerFrontend("test_cpp", createCppFrontend);  // ASSERT FAILURE
// }

int main() {
  test_singleton_identity();
  test_register_and_create();
  test_auto_select();
  test_auto_select_with_path();
  test_create_unknown_returns_nullptr();
  test_auto_select_unknown_extension_returns_nullptr();
  test_auto_select_no_extension_returns_nullptr();
  test_has_frontend();
  test_get_registered_names();
  test_extension_mapping();

  // V4: Duplicate registration triggers assert — tested via comment above.

  return 0;
}
```

---

## Part 5: 验收标准映射

| 验收标准 | 测试函数 | 验证内容 |
|----------|----------|----------|
| V1: 注册和创建 | `test_register_and_create()` | registerFrontend + create → FE != nullptr, getName()=="cpp" |
| V2: 自动选择 | `test_auto_select()` + `test_extension_mapping()` | addExtensionMapping + autoSelect → FE != nullptr |
| V3: 未知前端返回 nullptr | `test_create_unknown_returns_nullptr()` | create("nonexistent") → nullptr |
| V4: 重复注册触发 assert | 注释中验证 | 取消注释即触发 assert 失败 |
| 单例身份 | `test_singleton_identity()` | instance() 两次返回同一引用 |
| 无扩展名返回 nullptr | `test_auto_select_no_extension_returns_nullptr()` | 无点号的文件名 → nullptr |
| 未知扩展名返回 nullptr | `test_auto_select_unknown_extension_returns_nullptr()` | .xyz → nullptr |
| hasFrontend 查询 | `test_has_frontend()` | 已注册==true，未注册==false |
| getRegisteredNames 遍历 | `test_get_registered_names()` | 返回的列表包含所有已注册名称 |
| 带路径的自动选择 | `test_auto_select_with_path()` | `/path/to/source/hello.bt` 正确提取 .bt |

---

## Part 6: dev-tester 执行步骤

### 步骤 1：创建头文件

```bash
# 文件: include/blocktype/Frontend/FrontendRegistry.h
# 内容: 见 Part 1（直接复制完整代码）
```

### 步骤 2：创建实现文件

```bash
# 文件: src/Frontend/FrontendRegistry.cpp
# 内容: 见 Part 2（直接复制完整代码）
```

### 步骤 3：修改 CMakeLists.txt

```bash
# 修改 src/Frontend/CMakeLists.txt
# 在 add_library 源文件列表中添加 FrontendRegistry.cpp
# 见 Part 3 的 diff
```

### 步骤 4：创建测试文件

```bash
# 文件: tests/Frontend/test_frontend_registry.cpp
# 内容: 见 Part 4（直接复制完整代码）
```

### 步骤 5：编译验证

```bash
cd /Users/yuan/Documents/BlockType
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --target blocktypeFrontend
```

### 步骤 6：编译并运行测试

```bash
# 编译测试（具体命令根据 CMake 配置调整）
c++ -std=c++23 -I../include \
  ../tests/Frontend/test_frontend_registry.cpp \
  -L. -lblocktypeFrontend -lblocktype-ir -lblocktype-basic \
  -o test_frontend_registry

./test_frontend_registry
```

### 步骤 7：验证红线

```bash
# 确认以下检查点：
# [ ] 现有 blocktypeFrontend 库可正常编译
# [ ] 所有 B.1 测试仍然通过（test_frontend_base）
# [ ] 所有 B.2 测试通过（test_frontend_registry）
# [ ] FrontendRegistry 是全局单例
# [ ] create() 对未注册前端返回 nullptr（不抛异常）
# [ ] autoSelect() 对未知扩展名返回 nullptr（不抛异常）
```

### 步骤 8：完成

确认所有步骤通过后，通知 team-lead。

---

## 附录 A：规格中的 `ir::StringMap` 用法详解

项目的 `StringMap`（`blocktype/IR/ADT/StringMap.h`）与 LLVM 的 `StringMap` 接口有差异：

| 操作 | `ir::StringMap` API | 注意事项 |
|------|---------------------|----------|
| 插入 | `insert({key, value})` 返回 `{iterator, bool}` | `[]` 运算符也可用（不存在则默认构造） |
| 查找 | `find(key)` → `iterator`/`const_iterator` | key 类型为 `std::string` 或 `std::string_view` |
| 包含 | `contains(key)` → `bool` | 同上 |
| 遍历 | `begin()`/`end()` → `Entry&` 有 `.Key` 和 `.Value` | `Entry` 中 Key 为 `std::string` |
| 删除 | `erase(key)` → `size_t` | - |

**关键点**：
- `ir::StringMap` 的 `Entry::Key` 是 `std::string`，不是 `StringRef`
- `find()` 参数接受 `std::string` 或 `std::string_view`，不接受 `ir::StringRef`
- 遍历时通过 `Entry.Key` 和 `Entry.Value` 访问

因此在 `FrontendRegistry.cpp` 中：
- `find(Name.str())` 将 `ir::StringRef` 转为 `std::string`
- `insert({Key, Factory})` 使用 `std::string` 作为 key

---

## 附录 B：新增/修改文件清单

| 文件 | 操作 |
|------|------|
| `include/blocktype/Frontend/FrontendRegistry.h` | 新增 |
| `src/Frontend/FrontendRegistry.cpp` | 新增 |
| `tests/Frontend/test_frontend_registry.cpp` | 新增 |
| `src/Frontend/CMakeLists.txt` | 修改（添加 `FrontendRegistry.cpp`） |
