#pragma once

#include "blocktype/Basic/SourceLocation.h"
#include "blocktype/Basic/DiagnosticIDs.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

/// DiagnosticsEngine - Handles diagnostic reporting
class DiagnosticsEngine {
  unsigned NumErrors = 0;
  unsigned NumWarnings = 0;
  llvm::raw_ostream &OS;

public:
  explicit DiagnosticsEngine(llvm::raw_ostream &Out = llvm::errs())
    : OS(Out) {}

  /// Report a diagnostic with a custom message
  void report(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message);

  /// Report a diagnostic with a diagnostic ID
  void report(SourceLocation Loc, DiagID ID);

  /// Report a diagnostic with a diagnostic ID and additional text
  void report(SourceLocation Loc, DiagID ID, llvm::StringRef ExtraText);

  unsigned getNumErrors() const { return NumErrors; }
  unsigned getNumWarnings() const { return NumWarnings; }
  bool hasErrorOccurred() const { return NumErrors > 0; }
  void reset() { NumErrors = 0; NumWarnings = 0; }

  /// Get the severity level for a diagnostic ID
  static DiagLevel getDiagnosticLevel(DiagID ID);

  /// Get the message text for a diagnostic ID
  static const char* getDiagnosticMessage(DiagID ID);

private:
  void printDiagnostic(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message);
};

} // namespace blocktype
