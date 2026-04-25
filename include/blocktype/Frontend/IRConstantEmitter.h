//===--- IRConstantEmitter.h - IR Constant Emitter (Stub) ----*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
#define BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H

namespace blocktype {
namespace frontend {

class ASTToIRConverter;

/// IRConstantEmitter - Emits IR constants from AST expressions.
/// Stub class; implementation deferred to Task B.8.
class IRConstantEmitter {
public:
  explicit IRConstantEmitter(ASTToIRConverter& Converter) : Converter_(Converter) {}
  ~IRConstantEmitter() = default;

private:
  ASTToIRConverter& Converter_;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
