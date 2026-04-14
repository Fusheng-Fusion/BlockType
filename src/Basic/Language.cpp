#include "blocktype/Basic/Language.h"

namespace blocktype {

LanguageConfig LanguageConfig::get(Language L) {
  return L == Language::Chinese 
    ? LanguageConfig{Language::Chinese, "zh-CN", "中文"}
    : LanguageConfig{Language::English, "en-US", "English"};
}

Language LanguageManager::parseLanguage(llvm::StringRef Str) {
  return (Str == "zh" || Str == "zh-CN" || Str == "中文") 
    ? Language::Chinese : Language::English;
}

llvm::StringRef LanguageManager::languageToString(Language Lang) {
  return Lang == Language::Chinese ? "zh-CN" : "en-US";
}

} // namespace blocktype