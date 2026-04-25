#include "blocktype/IR/IRContract.h"

#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRConversionResult.h"
#include "blocktype/IR/IRType.h"
#include "blocktype/IR/IRValue.h"

namespace blocktype {
namespace ir {
namespace contract {

bool verifyIRModuleContract(const IRModule& M) {
  if (M.getName().empty())
    return false;
  return true;
}

bool verifyTypeCompleteness(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    auto* FTy = Func->getFunctionType();
    if (FTy->getReturnType()->isOpaque())
      return false;
    for (unsigned i = 0; i < FTy->getNumParams(); ++i) {
      if (FTy->getParamType(i)->isOpaque())
        return false;
    }
  }
  for (auto& GV : M.getGlobals()) {
    if (GV->getType()->isOpaque())
      return false;
  }
  return true;
}

bool verifyFunctionNonEmpty(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    if (Func->getNumBasicBlocks() == 0)
      return false;
  }
  return true;
}

bool verifyTerminatorContract(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    for (auto& BB : Func->getBasicBlocks()) {
      if (!BB->getTerminator())
        return false;
    }
  }
  return true;
}

bool verifyTypeConsistency(const IRModule& M) {
  for (auto& Func : M.getFunctions()) {
    if (Func->isDeclaration())
      continue;
    for (auto& BB : Func->getBasicBlocks()) {
      for (auto& Inst : BB->getInstList()) {
        if (Inst->isBinaryOp()) {
          if (Inst->getNumOperands() >= 2) {
            auto* LTy = Inst->getOperand(0)->getType();
            auto* RTy = Inst->getOperand(1)->getType();
            if (LTy && RTy && !LTy->equals(RTy))
              return false;
          }
        }
      }
    }
  }
  return true;
}

bool verifyTargetTripleValid(const IRModule& M) {
  StringRef Triple = M.getTargetTriple();
  if (Triple.empty())
    return true;
  if (Triple.find('-') == StringRef::npos)
    return false;
  return true;
}

IRVerificationResult verifyAllContracts(const IRModule& M) {
  IRVerificationResult Result(true);

  if (!verifyIRModuleContract(M))
    Result.addViolation("IRModule contract violated: empty module name");

  if (!verifyTypeCompleteness(M))
    Result.addViolation("Type completeness violated: opaque types in function signatures or globals");

  if (!verifyFunctionNonEmpty(M))
    Result.addViolation("Function non-empty violated: defined function has no basic blocks");

  if (!verifyTerminatorContract(M))
    Result.addViolation("Terminator contract violated: basic block missing terminator");

  if (!verifyTypeConsistency(M))
    Result.addViolation("Type consistency violated: operand type mismatch in binary operation");

  if (!verifyTargetTripleValid(M))
    Result.addViolation("Target triple invalid: malformed triple string");

  return Result;
}

} // namespace contract
} // namespace ir
} // namespace blocktype
