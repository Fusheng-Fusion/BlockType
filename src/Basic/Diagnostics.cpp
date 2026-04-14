#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

void DiagnosticsEngine::report(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message) {
  printDiagnostic(Loc, Level, Message);
  
  if (Level == DiagLevel::Error || Level == DiagLevel::Fatal)
    ++NumErrors;
  else if (Level == DiagLevel::Warning)
    ++NumWarnings;
}

void DiagnosticsEngine::report(SourceLocation Loc, DiagID ID) {
  DiagLevel Level = getDiagnosticLevel(ID);
  llvm::StringRef Message = getDiagnosticMessage(ID);
  report(Loc, Level, Message);
}

void DiagnosticsEngine::report(SourceLocation Loc, DiagID ID, llvm::StringRef ExtraText) {
  DiagLevel Level = getDiagnosticLevel(ID);
  std::string Message = std::string(getDiagnosticMessage(ID)) + ": " + ExtraText.str();
  report(Loc, Level, Message);
}

DiagLevel DiagnosticsEngine::getDiagnosticLevel(DiagID ID) {
  switch (ID) {
#define DIAG(ID, Level, Text) \
    case DiagID::ID: \
      return DiagLevel::Level;
#include "blocktype/Basic/DiagnosticIDs.def"
#undef DIAG
    default:
      return DiagLevel::Error;
  }
}

const char* DiagnosticsEngine::getDiagnosticMessage(DiagID ID) {
  switch (ID) {
#define DIAG(ID, Level, Text) \
    case DiagID::ID: \
      return Text;
#include "blocktype/Basic/DiagnosticIDs.def"
#undef DIAG
    default:
      return "unknown diagnostic";
  }
}

void DiagnosticsEngine::printDiagnostic(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message) {
  // Print location if valid
  if (Loc.isValid()) {
    // For now, just print a placeholder location
    // TODO: Integrate with SourceManager for actual file:line:column
    OS << "<input>";
  } else {
    OS << "<unknown>";
  }

  // Print severity
  switch (Level) {
    case DiagLevel::Ignored:
      OS << ": ignored: ";
      break;
    case DiagLevel::Note:
      OS << ": note: ";
      break;
    case DiagLevel::Remark:
      OS << ": remark: ";
      break;
    case DiagLevel::Warning:
      OS << ": warning: ";
      break;
    case DiagLevel::Error:
      OS << ": error: ";
      break;
    case DiagLevel::Fatal:
      OS << ": fatal error: ";
      break;
  }

  // Print message
  OS << Message << "\n";
}

} // namespace blocktype
