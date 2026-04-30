#include "blocktype/Frontend/PluginManager.h"

namespace blocktype {
namespace plugin {

bool PluginManager::registerPlugin(std::unique_ptr<CompilerPlugin> Plugin) {
  if (!Plugin) return false;
  auto Info = Plugin->getInfo();
  std::string Name = Info.Name;
  if (LoadedPlugins.contains(Name)) return false;
  LoadedPlugins[Name] = std::move(Plugin);
  return true;
}

bool PluginManager::unregisterPlugin(ir::StringRef Name) {
  return LoadedPlugins.erase(Name.str()) > 0;
}

CompilerPlugin* PluginManager::getPlugin(ir::StringRef Name) const {
  auto It = LoadedPlugins.find(Name.str());
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
