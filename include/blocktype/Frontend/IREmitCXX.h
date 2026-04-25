//===--- IREmitCXX.h - IR C++ Emitter (Stub) -----------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITCXX_H
#define BLOCKTYPE_FRONTEND_IREMITCXX_H

#include "blocktype/AST/Decl.h"

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitCXX - Emits IR for C++ specific constructs (vtables, RTTI, etc.).
/// Stub class; implementation deferred to Task B.9.
class IREmitCXX {
public:
  explicit IREmitCXX(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitCXX() = default;

  /// Emit vtable for the given CXXRecordDecl.
  void EmitVTable(CXXRecordDecl* RD) {
    // Stub: B.9 will implement
  }

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITCXX_H
