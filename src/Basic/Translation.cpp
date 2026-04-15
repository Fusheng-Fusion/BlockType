#include "blocktype/Basic/Translation.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/YAMLParser.h"

namespace blocktype {

// Helper function to parse a single translation entry from YAML mapping
static bool parseTranslationEntry(llvm::yaml::MappingNode* ValueNode,
                                   TranslationEntry& Entry) {
  if (ValueNode == nullptr) {
    return false;
  }

  for (auto& Field : *ValueNode) {
    auto* FieldKey = llvm::dyn_cast<llvm::yaml::ScalarNode>(Field.getKey());
    auto* FieldValue = llvm::dyn_cast<llvm::yaml::ScalarNode>(Field.getValue());

    if (FieldKey == nullptr || FieldValue == nullptr) {
      continue;
    }

    llvm::StringRef FieldName = FieldKey->getValue();
    llvm::StringRef FieldContent = FieldValue->getValue();

    if (FieldName == "message") {
      Entry.Message = FieldContent.str();
    } else if (FieldName == "note") {
      Entry.Note = FieldContent.str();
    }
  }

  return !Entry.Message.empty();
}

// Helper function to parse YAML document and populate translations
static void parseYAMLDocument(llvm::yaml::Document& Document,
                              llvm::StringMap<TranslationEntry>& Translations) {
  llvm::yaml::Node* Root = Document.getRoot();
  if (Root == nullptr) {
    return;
  }

  auto* Mapping = llvm::dyn_cast<llvm::yaml::MappingNode>(Root);
  if (Mapping == nullptr) {
    return;
  }

  for (auto& Entry : *Mapping) {
    auto* KeyNode = llvm::dyn_cast<llvm::yaml::ScalarNode>(Entry.getKey());
    if (KeyNode == nullptr) {
      continue;
    }

    llvm::StringRef DiagID = KeyNode->getValue();

    auto* ValueNode = llvm::dyn_cast<llvm::yaml::MappingNode>(Entry.getValue());
    TranslationEntry NewEntry;
    if (parseTranslationEntry(ValueNode, NewEntry)) {
      Translations[DiagID] = NewEntry;
    }
  }
}

bool TranslationManager::loadTranslations(Language Lang, llvm::StringRef Path) {
  // 读取文件
  auto BufferOrError = llvm::MemoryBuffer::getFile(Path);
  if (!BufferOrError) {
    return false;
  }

  auto& Translations = (Lang == Language::English) ? EnTranslations : ZhTranslations;

  // Parse YAML using LLVM's YAMLParser
  llvm::SourceMgr SourceManager;
  llvm::yaml::Stream YAMLStream(BufferOrError.get()->getBuffer(), SourceManager);

  // Iterate through documents (usually just one)
  for (auto& Document : YAMLStream) {
    parseYAMLDocument(Document, Translations);
  }

  return true;
}

llvm::StringRef TranslationManager::translate(llvm::StringRef Key) const {
  const auto& Translations = (CurrentLang == Language::English) ? EnTranslations : ZhTranslations;

  auto Iter = Translations.find(Key);
  if (Iter != Translations.end()) {
    return Iter->second.Message;
  }

  // 如果没有找到翻译，返回 key 本身
  return Key;
}

bool TranslationManager::hasTranslation(llvm::StringRef Key) const {
  const auto& Translations = (CurrentLang == Language::English) ? EnTranslations : ZhTranslations;
  return Translations.find(Key) != Translations.end();
}

} // namespace blocktype
