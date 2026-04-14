#pragma once

#include "blocktype/Basic/SourceLocation.h"
#include "llvm/ADT/StringRef.h"

namespace blocktype {

enum class DiagLevel {
  Ignored, Note, Remark, Warning, Error, Fatal
};

class DiagnosticsEngine {
  unsigned NumErrors = 0;
  unsigned NumWarnings = 0;
public:
  void report(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message);
  unsigned getNumErrors() const { return NumErrors; }
  unsigned getNumWarnings() const { return NumWarnings; }
  bool hasErrorOccurred() const { return NumErrors > 0; }
  void reset() { NumErrors = 0; NumWarnings = 0; }
};

} // namespace blocktype