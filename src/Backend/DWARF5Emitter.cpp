//===--- DWARF5Emitter.cpp - DWARF5 Debug Info Emitter -------*- C++ -*-===//

#include "blocktype/Backend/DebugInfoEmitter.h"
#include "blocktype/IR/IRModule.h"

namespace blocktype::backend {

bool DWARF5Emitter::emit(const ir::IRModule& M, ir::raw_ostream& OS) {
  // Stub: emit using DWARF5 format (default)
  return emitDWARF5(M, OS);
}

bool DWARF5Emitter::emitDWARF5(const ir::IRModule& M, ir::raw_ostream& OS) {
  // Stub: full implementation TBD (future Phase)
  (void)M;
  (void)OS;
  return true;
}

bool DWARF5Emitter::emitDWARF4(const ir::IRModule& M, ir::raw_ostream& OS) {
  // Not implemented — DWARF5 emitter does not support DWARF4
  (void)M;
  (void)OS;
  return false;
}

bool DWARF5Emitter::emitCodeView(const ir::IRModule& M, ir::raw_ostream& OS) {
  // Not implemented — CodeView is for Windows (far future)
  (void)M;
  (void)OS;
  return false;
}

} // namespace blocktype::backend
