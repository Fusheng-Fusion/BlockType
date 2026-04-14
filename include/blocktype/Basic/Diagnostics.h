#pragma once

#include "blocktype/Basic/SourceLocation.h"
#include "blocktype/Basic/DiagnosticIDs.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

class SourceManager;

/// Diagnostic language selection
enum class DiagnosticLanguage {
  English,
  Chinese,
  Auto  // Auto-detect from source file
};

/// DiagnosticsEngine - Handles diagnostic reporting with multi-language support
class DiagnosticsEngine {
  unsigned NumErrors = 0;
  unsigned NumWarnings = 0;
  llvm::raw_ostream &OS;
  const SourceManager *SM = nullptr;
  DiagnosticLanguage Lang = DiagnosticLanguage::English;

public:
  explicit DiagnosticsEngine(llvm::raw_ostream &Out = llvm::errs())
    : OS(Out) {}

  /// Set the source manager for location printing
  void setSourceManager(const SourceManager *Manager) { SM = Manager; }

  /// Set the diagnostic language
  void setLanguage(DiagnosticLanguage L) { Lang = L; }

  /// Get the current diagnostic language
  DiagnosticLanguage getLanguage() const { return Lang; }

  /// Report a diagnostic with a custom message
  void report(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message);

  /// Report a diagnostic with a diagnostic ID
  void report(SourceLocation Loc, DiagID ID);

  /// Report a diagnostic with a diagnostic ID and additional text
  void report(SourceLocation Loc, DiagID ID, llvm::StringRef ExtraText);

  /// Report a diagnostic with source range (for highlighting)
  void report(SourceLocation Loc, DiagID ID, SourceLocation RangeStart, 
              SourceLocation RangeEnd, llvm::StringRef ExtraText = "");

  unsigned getNumErrors() const { return NumErrors; }
  unsigned getNumWarnings() const { return NumWarnings; }
  bool hasErrorOccurred() const { return NumErrors > 0; }
  void reset() { NumErrors = 0; NumWarnings = 0; }

  /// Get the severity level for a diagnostic ID
  static DiagLevel getDiagnosticLevel(DiagID ID);

  /// Get the message text for a diagnostic ID (English)
  static const char* getDiagnosticMessage(DiagID ID);

  /// Get the message text for a diagnostic ID in specified language
  static const char* getDiagnosticMessage(DiagID ID, DiagnosticLanguage Lang);

  /// Get severity name in current language
  const char* getSeverityName(DiagLevel Level) const;

private:
  void printDiagnostic(SourceLocation Loc, DiagLevel Level, llvm::StringRef Message);
  void printSourceLine(SourceLocation Loc);
};

} // namespace blocktype
