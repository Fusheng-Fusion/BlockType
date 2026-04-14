#pragma once

#include "blocktype/Basic/Language.h"
#include "llvm/ADT/StringMap.h"
#include <string>

namespace blocktype {

struct TranslationEntry {
  std::string Message;
  std::string Note;
};

class TranslationManager {
  llvm::StringMap<TranslationEntry> EnTranslations;
  llvm::StringMap<TranslationEntry> ZhTranslations;
  Language CurrentLang = Language::English;
public:
  bool loadTranslations(Language Lang, llvm::StringRef Path);
  void setLanguage(Language Lang) { CurrentLang = Lang; }
  llvm::StringRef translate(llvm::StringRef Key) const;
  bool hasTranslation(llvm::StringRef Key) const;
};

} // namespace blocktype