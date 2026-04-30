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
