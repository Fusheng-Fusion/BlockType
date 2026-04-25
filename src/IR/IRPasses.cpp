#include "blocktype/IR/IRPasses.h"

#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRValue.h"

#include <algorithm>

namespace blocktype {
namespace ir {

// ============================================================================
// PassManager
// ============================================================================

bool PassManager::runAll(IRModule& M) {
  bool AnyChanged = false;
  for (auto& P : Passes_) {
    if (P->run(M)) {
      AnyChanged = true;
    }
  }
  return AnyChanged;
}

// ============================================================================
// DeadFunctionEliminationPass
// ============================================================================

bool DeadFunctionEliminationPass::run(IRModule& M) {
  auto& Functions = M.getFunctions();
  size_t OriginalSize = Functions.size();

  Functions.erase(
    std::remove_if(Functions.begin(), Functions.end(),
      [](const std::unique_ptr<IRFunction>& F) {
        return F->isDeclaration() &&
               F->getLinkage() == LinkageKind::Internal;
      }),
    Functions.end());

  return Functions.size() < OriginalSize;
}

// ============================================================================
// ConstantFoldingPass
// ============================================================================

bool ConstantFoldingPass::run(IRModule& M) {
  bool Changed = false;

  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;

    for (auto& BB : Func->getBasicBlocks()) {
      SmallVector<IRInstruction*, 16> ToFold;
      for (auto& Inst : BB->getInstList()) {
        if (auto Folded = tryFold(*Inst)) {
          (void)Folded;
          ToFold.push_back(Inst.get());
        }
      }

      for (auto* Inst : ToFold) {
        auto Folded = tryFold(*Inst);
        if (Folded) {
          Inst->replaceAllUsesWith(*Folded);
          BB->erase(Inst);
          Changed = true;
        }
      }
    }
  }

  return Changed;
}

std::optional<IRConstantInt*> ConstantFoldingPass::tryFold(IRInstruction& I) {
  Opcode Op = I.getOpcode();

  if (Op != Opcode::Add && Op != Opcode::Sub && Op != Opcode::Mul)
    return std::nullopt;

  if (I.getNumOperands() != 2)
    return std::nullopt;

  auto* LHS = I.getOperand(0);
  auto* RHS = I.getOperand(1);

  if (!IRConstantInt::classof(LHS) || !IRConstantInt::classof(RHS))
    return std::nullopt;

  auto* LC = static_cast<IRConstantInt*>(LHS);
  auto* RC = static_cast<IRConstantInt*>(RHS);

  unsigned BW = static_cast<IRIntegerType*>(LC->getType())->getBitWidth();
  if (static_cast<IRIntegerType*>(RC->getType())->getBitWidth() != BW)
    return std::nullopt;

  uint64_t Result = 0;
  switch (Op) {
  case Opcode::Add:
    Result = LC->getZExtValue() + RC->getZExtValue();
    break;
  case Opcode::Sub:
    Result = LC->getZExtValue() - RC->getZExtValue();
    break;
  case Opcode::Mul:
    Result = LC->getZExtValue() * RC->getZExtValue();
    break;
  default:
    return std::nullopt;
  }

  auto* FoldedConst = new IRConstantInt(static_cast<IRIntegerType*>(LC->getType()), Result);
  return FoldedConst;
}

// ============================================================================
// TypeCanonicalizationPass
// ============================================================================

bool TypeCanonicalizationPass::run(IRModule& M) {
  (void)M;
  return false;
}

} // namespace ir
} // namespace blocktype
