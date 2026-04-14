#include "blocktype/Basic/Diagnostics.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

void DiagnosticsEngine::report(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message) {
  switch (Level) {
    case DiagLevel::Error:
    case DiagLevel::Fatal:
      NumErrors++;
      llvm::errs() << "error: " << Message << "\n";
      break;
    case DiagLevel::Warning:
      NumWarnings++;
      llvm::errs() << "warning: " << Message << "\n";
      break;
    default:
      llvm::outs() << Message << "\n";
  }
}

} // namespace blocktype