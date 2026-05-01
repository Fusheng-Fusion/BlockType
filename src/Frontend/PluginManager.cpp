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
  return It != LoadedPlugins.end() ? (*It).second.get() : nullptr;
}

void PluginManager::registerPluginPasses(CompilerInstance& CI) {
  for (auto It = LoadedPlugins.begin(); It != LoadedPlugins.end(); ++It) {
    auto& Plugin = (*It).second;
    if (Plugin) Plugin->initialize(CI);
  }
}

void PluginManager::listPlugins(ir::raw_ostream& OS) const {
  for (auto It = LoadedPlugins.begin(); It != LoadedPlugins.end(); ++It) {
    auto& Plugin = (*It).second;
    if (Plugin) {
      auto Info = Plugin->getInfo();
      OS << Info.Name << " v" << Info.Version
         << " [" << static_cast<unsigned>(Info.Type) << "]"
         << " - " << Info.Description << "\n";
    }
  }
}

size_t PluginManager::getPluginCount() const {
  return LoadedPlugins.size();
}

// === IR Pass 插件注册实现 ===

bool PluginManager::registerPassCreator(ir::StringRef PassName, PassCreatorFn Creator) {
  std::string Key = PassName.str();
  if (PassCreators_.contains(Key)) return false;
  PassCreators_[Key] = std::move(Creator);
  return true;
}

const PassCreatorFn* PluginManager::getPassCreator(ir::StringRef PassName) const {
  auto It = PassCreators_.find(PassName.str());
  return It != PassCreators_.end() ? &(*It).second : nullptr;
}

unsigned PluginManager::createAndRegisterPasses() {
  if (!PM_) return 0;
  unsigned Count = 0;
  for (auto It = PassCreators_.begin(); It != PassCreators_.end(); ++It) {
    auto& Creator = (*It).second;
    if (Creator) {
      auto P = Creator();
      if (P) {
        PM_->addPassPtr(std::move(P));
        ++Count;
      }
    }
  }
  return Count;
}

} // namespace plugin
} // namespace blocktype
