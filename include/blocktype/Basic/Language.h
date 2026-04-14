#pragma once

#include "llvm/ADT/StringRef.h"

namespace blocktype {

enum class Language { English, Chinese };

struct LanguageConfig {
  Language Lang;
  llvm::StringRef Locale;      // "en-US" or "zh-CN"
  llvm::StringRef DisplayName; // "English" or "中文"
  static LanguageConfig get(Language L);
};

class LanguageManager {
  Language CurrentLang = Language::English;
public:
  void setLanguage(Language Lang) { CurrentLang = Lang; }
  Language getCurrentLanguage() const { return CurrentLang; }
  static Language parseLanguage(llvm::StringRef Str);
  static llvm::StringRef languageToString(Language Lang);
};

} // namespace blocktype