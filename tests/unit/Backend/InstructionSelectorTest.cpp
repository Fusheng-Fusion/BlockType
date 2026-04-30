//===--- InstructionSelectorTest.cpp - InstructionSelector tests -*- C++ -*-===//

#include <memory>

#include <gtest/gtest.h>

#include "blocktype/Backend/InstructionSelector.h"
#include "blocktype/IR/IRValue.h"
#include "blocktype/IR/IRType.h"

using namespace blocktype;
using namespace blocktype::backend;
using namespace blocktype::ir;

// ============================================================
// T1: LoweringRule creation
// ============================================================

TEST(InstructionSelectorTest, LoweringRuleDefaults) {
  LoweringRule R;
  EXPECT_EQ(R.SourceOp, Opcode::Ret);
  EXPECT_EQ(R.SourceDialect, dialect::DialectID::Core);
  EXPECT_TRUE(R.TargetPattern.empty());
  EXPECT_TRUE(R.Condition.empty());
  EXPECT_EQ(R.Priority, 0);
}

TEST(InstructionSelectorTest, LoweringRuleSetValues) {
  LoweringRule R;
  R.SourceOp = Opcode::Add;
  R.SourceDialect = dialect::DialectID::Core;
  R.TargetPattern = "add32 %x %y";
  R.Priority = 1;
  EXPECT_EQ(R.SourceOp, Opcode::Add);
  EXPECT_EQ(R.Priority, 1);
  EXPECT_EQ(R.SourceDialect, dialect::DialectID::Core);
  EXPECT_EQ(R.TargetPattern, "add32 %x %y");
}

// ============================================================
// T2: TargetInstruction default construction
// ============================================================

TEST(InstructionSelectorTest, TargetInstructionDefaults) {
  auto TI = std::make_unique<TargetInstruction>();
  EXPECT_TRUE(TI->getMnemonic().empty());
  EXPECT_TRUE(TI->getUsedRegs().empty());
  EXPECT_TRUE(TI->getDefRegs().empty());
  EXPECT_TRUE(TI->getIROperands().empty());
}

// ============================================================
// T3: TargetInstruction set values
// ============================================================

TEST(InstructionSelectorTest, TargetInstructionSetValues) {
  TargetInstruction TI;
  TI.setMnemonic("add");
  TI.addUsedReg(0);
  TI.addUsedReg(1);
  TI.addDefReg(2);

  EXPECT_EQ(TI.getMnemonic(), "add");
  EXPECT_EQ(TI.getUsedRegs().size(), 2u);
  EXPECT_EQ(TI.getUsedRegs()[0], 0u);
  EXPECT_EQ(TI.getUsedRegs()[1], 1u);
  EXPECT_EQ(TI.getDefRegs().size(), 1u);
  EXPECT_EQ(TI.getDefRegs()[0], 2u);
}
