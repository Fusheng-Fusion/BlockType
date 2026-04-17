#pragma once

namespace blocktype {

/// Diagnostic IDs for lexer and preprocessor
enum class DiagID {
#define DIAG(ENUM, LEVEL, EN_TEXT, ZH_TEXT) ENUM,
#include "blocktype/Basic/DiagnosticIDs.def"
#include "blocktype/Basic/DiagnosticSemaKinds.def"
#undef DIAG
  NUM_DIAGNOSTICS
};

/// Diagnostic severity level
enum class DiagLevel {
  Ignored,
  Note,
  Remark,
  Warning,
  Error,
  Fatal
};

/// Get the severity level for a diagnostic
DiagLevel getDiagnosticLevel(DiagID ID);

/// Get the message text for a diagnostic
const char* getDiagnosticMessage(DiagID ID);

} // namespace blocktype
