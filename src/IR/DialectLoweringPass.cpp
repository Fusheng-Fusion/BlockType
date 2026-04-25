#include "blocktype/IR/DialectLoweringPass.h"

#include <algorithm>
#include <memory>

#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRValue.h"

namespace blocktype {
namespace ir {

/// Helper: replace instruction I with NewInst in the same basic block.
/// Returns true on success.
static bool replaceInstruction(IRInstruction& I,
                                std::unique_ptr<IRInstruction> NewInst) {
  IRBasicBlock* BB = I.getParent();
  if (!BB) return false;

  auto& InstList = BB->getInstList();
  auto It = std::find_if(InstList.begin(), InstList.end(),
    [&I](const std::unique_ptr<IRInstruction>& P) { return P.get() == &I; });
  if (It == InstList.end()) return false;

  NewInst->setParent(BB);
  I.replaceAllUsesWith(NewInst.get());

  InstList.insert(It, std::move(NewInst));
  InstList.erase(It);
  return true;
}

/// Helper: erase instruction I from its basic block.
static bool eraseInstruction(IRInstruction& I) {
  IRBasicBlock* BB = I.getParent();
  if (!BB) return false;

  auto Removed = BB->erase(&I);
  return Removed != nullptr;
}

/// Helper: get parent IRFunction of an instruction.
static IRFunction* getParentFunction(IRInstruction& I) {
  IRBasicBlock* BB = I.getParent();
  if (!BB) return nullptr;
  return BB->getParent();
}

const SmallVector<DialectLoweringPass::LoweringRule, 16>&
DialectLoweringPass::getLoweringRules() {
  static const SmallVector<LoweringRule, 16> Rules = {
    // Rule 1: Invoke → Call (bt_cpp → bt_core)
    {
      dialect::DialectID::Cpp, Opcode::Invoke, dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        unsigned N = I.getNumOperands();
        if (N < 3) return false;

        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Call, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        for (unsigned i = 0; i + 2 < N; ++i) {
          NewInst->addOperand(I.getOperand(i));
        }
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 2: Resume → Unreachable (bt_cpp → bt_core)
    {
      dialect::DialectID::Cpp, Opcode::Resume, dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Unreachable, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 3: DynamicCast → Call (bt_cpp → bt_core)
    {
      dialect::DialectID::Cpp, Opcode::DynamicCast, dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        // TODO: 完整降级应生成 call @__bt_dynamic_cast(%obj, @typeinfo.src, @typeinfo.dst, %vtable_offset)
        //       并通过 IRModule::getOrInsertFunction 添加运行时辅助函数声明。当前为简化实现。
        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Call, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        for (unsigned i = 0; i < I.getNumOperands(); ++i) {
          NewInst->addOperand(I.getOperand(i));
        }
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 4: VtableDispatch → Call (bt_cpp → bt_core)
    {
      dialect::DialectID::Cpp, Opcode::VtableDispatch, dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        // TODO: 完整降级应生成 3 条指令序列：load vptr + gep idx + indirect_call。
        //       当前为简化实现，直接替换为 Call。
        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Call, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        for (unsigned i = 0; i < I.getNumOperands(); ++i) {
          NewInst->addOperand(I.getOperand(i));
        }
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 5: RTTITypeid → Call (bt_cpp → bt_core)
    {
      dialect::DialectID::Cpp, Opcode::RTTITypeid, dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        // TODO: 完整降级应生成 call @__bt_typeid(%expr)，引入运行时辅助函数。
        //       当前为简化实现。
        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Call, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        for (unsigned i = 0; i < I.getNumOperands(); ++i) {
          NewInst->addOperand(I.getOperand(i));
        }
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 6: TargetIntrinsic → Call (bt_target → bt_core)
    {
      dialect::DialectID::Target, Opcode::TargetIntrinsic,
      dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        // TODO: 完整降级应将第一个操作数作为函数名直接调用。
        //       当前为简化实现，操作数直接复制。
        auto NewInst = std::make_unique<IRInstruction>(
          Opcode::Call, I.getType(), I.getValueID(),
          dialect::DialectID::Core, I.getName());
        for (unsigned i = 0; i < I.getNumOperands(); ++i) {
          NewInst->addOperand(I.getOperand(i));
        }
        return replaceInstruction(I, std::move(NewInst));
      }
    },

    // Rule 7: MetaInlineAlways → function attr (bt_meta → bt_core)
    {
      dialect::DialectID::Metadata, Opcode::MetaInlineAlways,
      dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        IRFunction* F = getParentFunction(I);
        if (!F) return false;
        F->addAttribute(FunctionAttr::AlwaysInline);
        return eraseInstruction(I);
      }
    },

    // Rule 8: MetaInlineNever → function attr (bt_meta → bt_core)
    {
      dialect::DialectID::Metadata, Opcode::MetaInlineNever,
      dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        IRFunction* F = getParentFunction(I);
        if (!F) return false;
        F->addAttribute(FunctionAttr::NoInline);
        return eraseInstruction(I);
      }
    },

    // Rule 9: MetaHot → function attr (bt_meta → bt_core)
    {
      dialect::DialectID::Metadata, Opcode::MetaHot,
      dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        IRFunction* F = getParentFunction(I);
        if (!F) return false;
        F->addAttribute(FunctionAttr::Hot);
        return eraseInstruction(I);
      }
    },

    // Rule 10: MetaCold → function attr (bt_meta → bt_core)
    {
      dialect::DialectID::Metadata, Opcode::MetaCold,
      dialect::DialectID::Core,
      [](IRInstruction& I) -> bool {
        IRFunction* F = getParentFunction(I);
        if (!F) return false;
        F->addAttribute(FunctionAttr::Cold);
        return eraseInstruction(I);
      }
    },
  };
  return Rules;
}

bool DialectLoweringPass::needsLowering(const IRInstruction& I) const {
  dialect::DialectID D = I.getDialect();
  // Core 和 Debug Dialect 不需要降级
  if (D == dialect::DialectID::Core || D == dialect::DialectID::Debug)
    return false;
  return !BackendCap_.hasDialect(D);
}

bool DialectLoweringPass::lowerInstruction(IRInstruction& I) const {
  const auto& Rules = getLoweringRules();
  for (const auto& Rule : Rules) {
    if (I.getDialect() == Rule.SourceDialect &&
        I.getOpcode() == Rule.SourceOpcode) {
      return Rule.Lower(I);
    }
  }
  // 没有匹配的降级规则
  return false;
}

bool DialectLoweringPass::run(IRModule& M) {
  bool Changed = false;

  for (auto& Func : M.getFunctions()) {
    for (auto& BB : Func->getBasicBlocks()) {
      auto It = BB->getInstList().begin();
      while (It != BB->getInstList().end()) {
        IRInstruction* I = It->get();
        // Save next iterator before potential modification
        auto Next = std::next(It);
        if (needsLowering(*I)) {
          if (lowerInstruction(*I)) {
            Changed = true;
          }
        }
        It = Next;
      }
    }
  }
  return Changed;
}

} // namespace ir
} // namespace blocktype
