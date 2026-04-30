//===--- LLVMBackendRegistration.cpp - Static Registration -*- C++ -*-===//

#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/Backend/LLVMBackend.h"

namespace blocktype::backend {

/// Static registrator — auto-registers the LLVM backend at program start.
static struct LLVMBackendRegistrator {
  LLVMBackendRegistrator() {
    BackendRegistry::instance().registerBackend("llvm", createLLVMBackend);
  }
} LLVMBackendRegistratorInstance;

} // namespace blocktype::backend
