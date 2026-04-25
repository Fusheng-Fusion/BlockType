//===--- IRConstantEmitter.cpp - IR Constant Emitter ----------*- C++ -*-===//

#include "blocktype/Frontend/IRConstantEmitter.h"
#include "blocktype/IR/IRType.h"

namespace blocktype {
namespace frontend {

//===----------------------------------------------------------------------===//
// Integer constants
//===----------------------------------------------------------------------===//

ir::IRConstantInt*
IRConstantEmitter::EmitConstInt(const ir::APInt& Val) {
  ir::IRIntegerType* Ty = TypeCtx_.getIntType(Val.getBitWidth());
  return IRCtx_.create<ir::IRConstantInt>(Ty, Val);
}

ir::IRConstantInt*
IRConstantEmitter::EmitConstInt(uint64_t Val, unsigned BitWidth, bool IsSigned) {
  ir::APInt AP(BitWidth, Val, IsSigned);
  return EmitConstInt(AP);
}

//===----------------------------------------------------------------------===//
// Floating-point constants
//===----------------------------------------------------------------------===//

ir::IRConstantFP*
IRConstantEmitter::EmitConstFloat(const ir::APFloat& Val) {
  unsigned BitWidth = Val.getBitWidth();
  ir::IRFloatType* Ty = TypeCtx_.getFloatType(BitWidth);
  return IRCtx_.create<ir::IRConstantFP>(Ty, Val);
}

ir::IRConstantFP*
IRConstantEmitter::EmitConstFloat(double Val, unsigned BitWidth) {
  ir::APFloat::Semantics Sem =
      (BitWidth <= 32) ? ir::APFloat::Semantics::Float
                       : ir::APFloat::Semantics::Double;
  ir::APFloat APF(Sem, Val);
  return EmitConstFloat(APF);
}

//===----------------------------------------------------------------------===//
// Special constants
//===----------------------------------------------------------------------===//

ir::IRConstantNull*
IRConstantEmitter::EmitConstNull(ir::IRType* T) {
  return IRCtx_.create<ir::IRConstantNull>(T);
}

ir::IRConstantUndef*
IRConstantEmitter::EmitConstUndef(ir::IRType* T) {
  return ir::IRConstantUndef::get(IRCtx_, T);
}

ir::IRConstantAggregateZero*
IRConstantEmitter::EmitConstAggregateZero(ir::IRType* T) {
  return IRCtx_.create<ir::IRConstantAggregateZero>(T);
}

//===----------------------------------------------------------------------===//
// Aggregate constants
//===----------------------------------------------------------------------===//

ir::IRConstantStruct*
IRConstantEmitter::EmitConstStruct(ir::IRStructType* Ty,
                                    ir::SmallVector<ir::IRConstant*, 16> Vals) {
  assert(Ty->getNumFields() == Vals.size() && "Struct field count mismatch");
  return IRCtx_.create<ir::IRConstantStruct>(Ty, std::move(Vals));
}

ir::IRConstantStruct*
IRConstantEmitter::EmitConstStruct(ir::IRStructType* Ty,
                                    ir::ArrayRef<ir::IRConstant*> Vals) {
  ir::SmallVector<ir::IRConstant*, 16> SV;
  for (auto* V : Vals)
    SV.push_back(V);
  return EmitConstStruct(Ty, std::move(SV));
}

ir::IRConstantArray*
IRConstantEmitter::EmitConstArray(ir::IRArrayType* Ty,
                                   ir::SmallVector<ir::IRConstant*, 16> Vals) {
  return IRCtx_.create<ir::IRConstantArray>(Ty, std::move(Vals));
}

ir::IRConstantArray*
IRConstantEmitter::EmitConstArray(ir::IRArrayType* Ty,
                                   ir::ArrayRef<ir::IRConstant*> Vals) {
  ir::SmallVector<ir::IRConstant*, 16> SV;
  for (auto* V : Vals)
    SV.push_back(V);
  return EmitConstArray(Ty, std::move(SV));
}

} // namespace frontend
} // namespace blocktype
