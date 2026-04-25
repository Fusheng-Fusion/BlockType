//===--- IREmitExpr.h - IR Expression Emitter (Stub) ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Stub header for IREmitExpr. Full implementation is Task B.5.
// This file exists so that ASTToIRConverter can forward-declare the class.
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IREMITEEXPR_H
#define BLOCKTYPE_FRONTEND_IREMITEEXPR_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IREmitExpr - Emits IR for AST expressions.
/// Stub class; implementation deferred to Task B.5.
class IREmitExpr {
public:
  explicit IREmitExpr(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IREmitExpr() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IREMITEEXPR_H
