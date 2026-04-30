# Task B-F1：PluginManager 插件管理器（优化版）

## 1. 代码库验证摘要（修正列表）

### 已验证类型

| 规格中的类型 | 实际代码库 | 状态 | 修正 |
|-------------|-----------|------|------|
| `CompilerInstance` | `blocktype::CompilerInstance`（`Frontend/CompilerInstance.h`） | ✅ 存在 | 直接使用 |
| `DenseMap` | `ir::DenseMap`（`IR/ADT/DenseMap.h`，通过 `IR/ADT.h` 引入） | ✅ 存在 | 使用 `ir::DenseMap` |
| `StringRef` | `ir::StringRef`（`IR/ADT/StringRef.h`，通过 `IR/ADT.h` 引入） | ✅ 存在 | 使用 `ir::StringRef` |
| `raw_ostream` | `ir::raw_ostream`（`IR/ADT/raw_ostream.h`，通过 `IR/ADT.h` 引入） | ✅ 存在 | 使用 `ir::raw_ostream` |
| `SmallVector` | `ir::SmallVector`（`IR/ADT/SmallVector.h`，通过 `IR/ADT.h` 引入） | ✅ 存在 | 使用 `ir::SmallVector` |

### 关键修正

1. **`loadPlugin()` 简化**：原始规格要求加载 `.so` 动态库文件，但 BlockType 项目不依赖动态库加载机制。改为内存注册模式——插件通过 `registerPlugin()` 直接注册 `unique_ptr<CompilerPlugin>`，不涉及 `dlopen/dlsym`。
2. **移除 `LoadedLibraries`**：因为不再加载 `.so`，删除 `std::vector<void*> LoadedLibraries` 字段。
3. **`raw_ostream` 命名空间**：规格写 `raw_ostream`，实际为 `ir::raw_ostream`（来自 `blocktype/IR/ADT.h`）。

---

## 2. 完整类定义（精确 API 签名）

### 头文件：`include/blocktype/Frontend/PluginManager.h`

```cpp
#ifndef BLOCKTYPE_FRONTEND_PLUGINMANAGER_H
#define BLOCKTYPE_FRONTEND_PLUGINMANAGER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "blocktype/IR/ADT.h"
#include "blocktype/Frontend/CompilerInstance.h"

namespace blocktype {
namespace plugin {

// ============================================================
// PluginType — 插件类型枚举
// ============================================================

enum class PluginType : uint8_t {
  IRPass      = 0,
  Frontend    = 1,
  Backend     = 2,
  Diagnostic  = 3,
  Analysis    = 4,
};

// ============================================================
// PluginInfo — 插件元信息
// ============================================================

struct PluginInfo {
  std::string Name;
  std::string Version;
  std::string Description;
  PluginType Type;
  std::vector<std::string> ProvidedPasses;
  std::vector<std::string> RequiredDialects;
};

// ============================================================
// CompilerPlugin — 编译器插件抽象基类
// ============================================================

class CompilerPlugin {
public:
  virtual ~CompilerPlugin() = default;
  virtual PluginInfo getInfo() const = 0;
  virtual bool initialize(CompilerInstance& CI) = 0;
  virtual void finalize() = 0;
};

// ============================================================
// PluginManager — 插件管理器（内存注册模式）
// ============================================================

class PluginManager {
  ir::DenseMap<ir::StringRef, std::unique_ptr<CompilerPlugin>> LoadedPlugins;

public:
  /// 注册一个插件（接管所有权）。成功返回 true，重名返回 false。
  bool registerPlugin(std::unique_ptr<CompilerPlugin> Plugin);

  /// 注销并移除指定名称的插件。成功返回 true。
  bool unregisterPlugin(ir::StringRef Name);

  /// 按名称查找插件（不转移所有权）。
  CompilerPlugin* getPlugin(ir::StringRef Name) const;

  /// 将所有已注册插件的 Pass 注册到 CompilerInstance。
  void registerPluginPasses(CompilerInstance& CI);

  /// 列出所有已注册插件的信息到输出流。
  void listPlugins(ir::raw_ostream& OS) const;

  /// 返回已注册插件数量。
  size_t getPluginCount() const;
};

} // namespace plugin
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_PLUGINMANAGER_H
```

### 实现文件：`src/Frontend/PluginManager.cpp`

```cpp
#include "blocktype/Frontend/PluginManager.h"

namespace blocktype {
namespace plugin {

bool PluginManager::registerPlugin(std::unique_ptr<CompilerPlugin> Plugin) {
  if (!Plugin) return false;
  auto Info = Plugin->getInfo();
  ir::StringRef Name = Info.Name;
  if (LoadedPlugins.count(Name) > 0) return false;
  LoadedPlugins[Name] = std::move(Plugin);
  return true;
}

bool PluginManager::unregisterPlugin(ir::StringRef Name) {
  return LoadedPlugins.erase(Name) > 0;
}

CompilerPlugin* PluginManager::getPlugin(ir::StringRef Name) const {
  auto It = LoadedPlugins.find(Name);
  return It != LoadedPlugins.end() ? It->second.get() : nullptr;
}

void PluginManager::registerPluginPasses(CompilerInstance& CI) {
  for (auto& [Name, Plugin] : LoadedPlugins) {
    Plugin->initialize(CI);
  }
}

void PluginManager::listPlugins(ir::raw_ostream& OS) const {
  for (auto& [Name, Plugin] : LoadedPlugins) {
    auto Info = Plugin->getInfo();
    OS << Info.Name << " v" << Info.Version
       << " [" << static_cast<unsigned>(Info.Type) << "]"
       << " - " << Info.Description << "\n";
  }
}

size_t PluginManager::getPluginCount() const {
  return LoadedPlugins.size();
}

} // namespace plugin
} // namespace blocktype
```

---

## 3. 产出文件列表

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Frontend/PluginManager.h` |
| 新增 | `src/Frontend/PluginManager.cpp` |
| 新增 | `tests/unit/Frontend/PluginManagerTest.cpp` |

---

## 4. CMakeLists.txt 修改

### `src/Frontend/CMakeLists.txt`

在 `add_library(blocktypeFrontend ...)` 的源文件列表中添加：

```
PluginManager.cpp
```

### `tests/unit/Frontend/CMakeLists.txt`

追加以下内容：

```cmake
# PluginManager unit test (GTest)

add_executable(PluginManagerTest
  PluginManagerTest.cpp
)

target_link_libraries(PluginManagerTest
  PRIVATE
    blocktypeFrontend
    blocktype-basic
    blocktype-ir
    ${LLVM_LIBS}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(PluginManagerTest
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(PluginManagerTest)
```

---

## 5. GTest 测试用例

### `tests/unit/Frontend/PluginManagerTest.cpp`

```cpp
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "blocktype/Frontend/PluginManager.h"

using namespace blocktype;
using namespace blocktype::plugin;

// ============================================================
// 测试用辅助插件
// ============================================================

class MockPlugin : public CompilerPlugin {
  PluginInfo Info_;
  bool Initialized_ = false;
public:
  explicit MockPlugin(std::string Name, PluginType Type = PluginType::IRPass)
    : Info_{std::move(Name), "1.0.0", "mock plugin", Type, {}, {}} {}

  PluginInfo getInfo() const override { return Info_; }

  bool initialize(CompilerInstance& /*CI*/) override {
    Initialized_ = true;
    return true;
  }

  void finalize() override { Initialized_ = false; }

  bool isInitialized() const { return Initialized_; }
};

// ============================================================
// 测试：PluginInfo 默认构造
// ============================================================

TEST(PluginManagerTest, PluginInfoDefaultConstruct) {
  PluginInfo Info;
  Info.Name = "test-pass";
  Info.Type = PluginType::IRPass;
  Info.ProvidedPasses = {"my-pass"};
  EXPECT_EQ(Info.Name, "test-pass");
  EXPECT_EQ(Info.Type, PluginType::IRPass);
  EXPECT_EQ(Info.ProvidedPasses.size(), 1u);
}

// ============================================================
// 测试：注册和查询插件
// ============================================================

TEST(PluginManagerTest, RegisterAndGetPlugin) {
  PluginManager PM;
  auto Plugin = std::make_unique<MockPlugin>("test-plugin");
  EXPECT_TRUE(PM.registerPlugin(std::move(Plugin)));
  EXPECT_NE(PM.getPlugin("test-plugin"), nullptr);

  auto Info = PM.getPlugin("test-plugin")->getInfo();
  EXPECT_EQ(Info.Name, "test-plugin");
  EXPECT_EQ(Info.Version, "1.0.0");
}

// ============================================================
// 测试：查询不存在的插件
// ============================================================

TEST(PluginManagerTest, GetNonexistentPlugin) {
  PluginManager PM;
  EXPECT_EQ(PM.getPlugin("nonexistent"), nullptr);
}

// ============================================================
// 测试：重复注册同名插件失败
// ============================================================

TEST(PluginManagerTest, DuplicateRegistrationFails) {
  PluginManager PM;
  EXPECT_TRUE(PM.registerPlugin(std::make_unique<MockPlugin>("dup")));
  EXPECT_FALSE(PM.registerPlugin(std::make_unique<MockPlugin>("dup")));
  EXPECT_EQ(PM.getPluginCount(), 1u);
}

// ============================================================
// 测试：注册空指针失败
// ============================================================

TEST(PluginManagerTest, RegisterNullFails) {
  PluginManager PM;
  EXPECT_FALSE(PM.registerPlugin(nullptr));
}

// ============================================================
// 测试：注销插件
// ============================================================

TEST(PluginManagerTest, UnregisterPlugin) {
  PluginManager PM;
  PM.registerPlugin(std::make_unique<MockPlugin>("to-remove"));
  EXPECT_TRUE(PM.unregisterPlugin("to-remove"));
  EXPECT_EQ(PM.getPlugin("to-remove"), nullptr);
  EXPECT_FALSE(PM.unregisterPlugin("to-remove")); // 二次注销失败
}

// ============================================================
// 测试：插件计数
// ============================================================

TEST(PluginManagerTest, PluginCount) {
  PluginManager PM;
  EXPECT_EQ(PM.getPluginCount(), 0u);
  PM.registerPlugin(std::make_unique<MockPlugin>("a"));
  PM.registerPlugin(std::make_unique<MockPlugin>("b"));
  EXPECT_EQ(PM.getPluginCount(), 2u);
}

// ============================================================
// 测试：PluginType 枚举值
// ============================================================

TEST(PluginManagerTest, PluginTypeEnumValues) {
  EXPECT_EQ(static_cast<uint8_t>(PluginType::IRPass), 0);
  EXPECT_EQ(static_cast<uint8_t>(PluginType::Frontend), 1);
  EXPECT_EQ(static_cast<uint8_t>(PluginType::Backend), 2);
  EXPECT_EQ(static_cast<uint8_t>(PluginType::Diagnostic), 3);
  EXPECT_EQ(static_cast<uint8_t>(PluginType::Analysis), 4);
}
```

---

## 6. 验收标准

1. `plugin::PluginInfo` 可构造并存储 Name/Version/Description/Type/ProvidedPasses/RequiredDialects
2. `plugin::PluginManager::registerPlugin()` 可注册插件，重复注册返回 false
3. `plugin::PluginManager::getPlugin()` 可查询已注册插件，不存在返回 nullptr
4. `plugin::PluginManager::unregisterPlugin()` 可注销插件
5. `plugin::PluginManager::listPlugins()` 可输出插件列表到 `ir::raw_ostream`
6. `plugin::PluginManager::getPluginCount()` 返回已注册数量
7. 所有 GTest 测试通过
8. 不修改 `FrontendRegistry`，插件系统是上层管理器
