#include "blocktype/Basic/Translation.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/FileSystem.h"

namespace blocktype {

bool TranslationManager::loadTranslations(Language Lang, llvm::StringRef Path) {
  // 读取文件
  auto BufferOrError = llvm::MemoryBuffer::getFile(Path);
  if (!BufferOrError) {
    return false;
  }
  
  // TODO: 实现 YAML 解析
  // 当前版本使用简化实现
  
  auto& Translations = (Lang == Language::English) ? EnTranslations : ZhTranslations;
  
  // 示例翻译条目
  if (Lang == Language::Chinese) {
    TranslationEntry Entry;
    Entry.Message = "未声明的变量";
    Entry.Note = "请确保在使用前声明该变量";
    Translations["err_undeclared_var"] = Entry;
    
    Entry.Message = "类型不匹配";
    Entry.Note = "请检查表达式类型";
    Translations["err_type_mismatch"] = Entry;
  }
  
  return true;
}

llvm::StringRef TranslationManager::translate(llvm::StringRef Key) const {
  const auto& Translations = (CurrentLang == Language::English) ? EnTranslations : ZhTranslations;
  
  auto It = Translations.find(Key);
  if (It != Translations.end()) {
    return It->second.Message;
  }
  
  // 如果没有找到翻译，返回 key 本身
  return Key;
}

bool TranslationManager::hasTranslation(llvm::StringRef Key) const {
  const auto& Translations = (CurrentLang == Language::English) ? EnTranslations : ZhTranslations;
  return Translations.find(Key) != Translations.end();
}

} // namespace blocktype
