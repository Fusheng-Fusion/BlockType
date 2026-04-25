//===--- IRConstantEmitter.h - IR Constant Emitter ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
#define BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRTypeContext.h"
#include "blocktype/IR/IRConstant.h"

namespace blocktype {
namespace frontend {

/// IRConstantEmitter - Emits IR constants (backend-independent).
///
/// Provides a unified API for creating all kinds of IR constants.
/// Uses IRContext::create<T>() for arena allocation and IRTypeContext
/// for type queries.
class IRConstantEmitter {
  ir::IRContext& IRCtx_;
  ir::IRTypeContext& TypeCtx_;

public:
  explicit IRConstantEmitter(ir::IRContext& C, ir::IRTypeContext& TC)
    : IRCtx_(C), TypeCtx_(TC) {}

  ~IRConstantEmitter() = default;

  // Non-copyable
  IRConstantEmitter(const IRConstantEmitter&) = delete;
  IRConstantEmitter& operator=(const IRConstantEmitter&) = delete;

  //===--- Integer constants ---===//

  /// Emit integer constant from APInt.
  ir::IRConstantInt* EmitConstInt(const ir::APInt& Val);

  /// Emit integer constant (convenience overload).
  ir::IRConstantInt* EmitConstInt(uint64_t Val, unsigned BitWidth,
                                  bool IsSigned = false);

  //===--- Floating-point constants ---===//

  /// Emit floating-point constant from APFloat.
  ir::IRConstantFP* EmitConstFloat(const ir::APFloat& Val);

  /// Emit floating-point constant (convenience overload).
  ir::IRConstantFP* EmitConstFloat(double Val, unsigned BitWidth = 64);

  //===--- Special constants ---===//

  /// Emit null pointer constant.
  ir::IRConstantNull* EmitConstNull(ir::IRType* T);

  /// Emit undefined value constant. Uses IRConstantUndef::get() for caching.
  ir::IRConstantUndef* EmitConstUndef(ir::IRType* T);

  /// Emit aggregate zero constant (for default initialization).
  ir::IRConstantAggregateZero* EmitConstAggregateZero(ir::IRType* T);

  //===--- Aggregate constants ---===//

  /// Emit struct constant.
  ir::IRConstantStruct* EmitConstStruct(ir::IRStructType* Ty,
                                        ir::SmallVector<ir::IRConstant*, 16> Vals);

  /// Emit struct constant (ArrayRef overload).
  ir::IRConstantStruct* EmitConstStruct(ir::IRStructType* Ty,
                                        ir::ArrayRef<ir::IRConstant*> Vals);

  /// Emit array constant.
  ir::IRConstantArray* EmitConstArray(ir::IRArrayType* Ty,
                                      ir::SmallVector<ir::IRConstant*, 16> Vals);

  /// Emit array constant (ArrayRef overload).
  ir::IRConstantArray* EmitConstArray(ir::IRArrayType* Ty,
                                      ir::ArrayRef<ir::IRConstant*> Vals);

  //===--- Accessors ---===//

  ir::IRContext& getIRContext() { return IRCtx_; }
  ir::IRTypeContext& getTypeContext() { return TypeCtx_; }
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IRCONSTANTEMITTER_H
