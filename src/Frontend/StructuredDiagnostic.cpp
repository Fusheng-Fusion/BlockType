#include "blocktype/Frontend/StructuredDiagnostic.h"

#include <sstream>

namespace blocktype {
namespace diag {

void DetailedDiagEmitter::emit(const StructuredDiagnostic& D) {
  // 将基类引用尝试转换为 DetailedStructuredDiagnostic
  // 存储副本用于后续 SARIF/JSON/Text 输出
  const auto* Detailed = dynamic_cast<const DetailedStructuredDiagnostic*>(&D);
  if (Detailed) {
    LastDiag_ = *Detailed;
    HasLast_ = true;
  }
}

void DetailedDiagEmitter::emitSARIF(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  // SARIF 2.1 简化格式
  OS << "{\"$schema\":\"https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json\",\n";
  OS << " \"runs\":[{\"results\":[{\n";
  OS << "   \"level\":\"" << getDiagnosticLevelName(LastDiag_.Level) << "\",\n";
  OS << "   \"message\":{\"text\":\"" << LastDiag_.Message << "\"},\n";
  if (!LastDiag_.Category.empty()) {
    OS << "   \"ruleId\":\"" << LastDiag_.Category << "\",\n";
  }
  OS << "   \"ruleIndex\":" << static_cast<uint32_t>(LastDiag_.Code) << "\n";
  OS << "  }]}\n]}\n";
}

void DetailedDiagEmitter::emitJSON(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  OS << "{\n";
  OS << "  \"level\": \"" << getDiagnosticLevelName(LastDiag_.Level) << "\",\n";
  OS << "  \"code\": " << static_cast<uint32_t>(LastDiag_.Code) << ",\n";
  OS << "  \"group\": \"" << getDiagnosticGroupName(LastDiag_.Group) << "\",\n";
  OS << "  \"message\": \"" << LastDiag_.Message << "\"";
  if (!LastDiag_.Category.empty()) {
    OS << ",\n  \"category\": \"" << LastDiag_.Category << "\"";
  }
  if (!LastDiag_.FlagName.empty()) {
    OS << ",\n  \"flag\": \"" << LastDiag_.FlagName << "\"";
  }
  if (LastDiag_.IRRelatedDialect.has_value()) {
    OS << ",\n  \"dialect\": " << static_cast<unsigned>(*LastDiag_.IRRelatedDialect);
  }
  if (LastDiag_.IRRelatedOpcode.has_value()) {
    OS << ",\n  \"opcode\": " << static_cast<unsigned>(*LastDiag_.IRRelatedOpcode);
  }
  if (!LastDiag_.FixIts.empty()) {
    OS << ",\n  \"fixits\": [";
    for (size_t i = 0; i < LastDiag_.FixIts.size(); ++i) {
      if (i > 0) OS << ", ";
      OS << "{\"replacement\":\"" << LastDiag_.FixIts[i].Replacement
         << "\",\"description\":\"" << LastDiag_.FixIts[i].Description << "\"}";
    }
    OS << "]";
  }
  OS << "\n}\n";
}

void DetailedDiagEmitter::emitText(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  // 文本格式：<level>: <message> [code=<code>]
  OS << getDiagnosticLevelName(LastDiag_.Level) << ": " << LastDiag_.Message;
  OS << " [code=0x" << std::hex << static_cast<uint32_t>(LastDiag_.Code) << std::dec << "]";
  if (!LastDiag_.Category.empty()) {
    OS << " [" << LastDiag_.Category << "]";
  }
  if (!LastDiag_.FlagName.empty()) {
    OS << " [" << LastDiag_.FlagName << "]";
  }
  OS << "\n";
}

} // namespace diag
} // namespace blocktype
