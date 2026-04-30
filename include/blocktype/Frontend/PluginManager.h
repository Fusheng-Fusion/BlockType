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
  // NOTE: Using std::string as key because ir::StringRef is non-owning
  // and cannot be safely stored as a DenseMap key (no std::hash, dangling risk).
  ir::DenseMap<std::string, std::unique_ptr<CompilerPlugin>> LoadedPlugins;

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
