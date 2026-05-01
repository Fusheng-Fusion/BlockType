#ifndef BLOCKTYPE_FRONTEND_PLUGINMACROS_H
#define BLOCKTYPE_FRONTEND_PLUGINMACROS_H

#include "blocktype/Frontend/PluginManager.h"

/// BTIR_PASS_PLUGIN — 在编译期将一个 Pass 类注册到 PluginManager。
/// PassClass: Pass 子类名（需可默认构造）
/// PluginName: Pass 的字符串标识（ir::StringRef 兼容）
/// PMgr: PluginManager 引用
#define BTIR_PASS_PLUGIN(PassClass, PluginName, PMgr) \
  static bool PassClass##_registered_ = \
    (PMgr).registerPassCreator(PluginName, \
      []() -> std::unique_ptr<blocktype::ir::Pass> { \
        return std::make_unique<PassClass>(); \
      });

#endif // BLOCKTYPE_FRONTEND_PLUGINMACROS_H
