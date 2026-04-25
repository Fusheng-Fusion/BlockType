//===--- IREmitStmt.h - IR Statement Emitter (Stub) ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITSTMT_H
#define BLOCKTYPE_FRONTEND_IREMITSTMT_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitStmt - Emits IR for AST statements.
/// Stub class; implementation deferred to Task B.6.
class IREmitStmt {
public:
  explicit IREmitStmt(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitStmt() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITSTMT_H
