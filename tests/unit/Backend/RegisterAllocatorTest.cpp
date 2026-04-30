//===--- RegisterAllocatorTest.cpp - RegisterAllocator tests -*- C++ -*-===//

#include <memory>

#include <gtest/gtest.h>

#include "blocktype/Backend/RegisterAllocator.h"

using namespace blocktype;
using namespace blocktype::backend;

// ============================================================
// T1: RegAllocFactory creates Greedy allocator
// ============================================================

TEST(RegisterAllocatorTest, CreateGreedyAllocator) {
  auto RA = RegAllocFactory::create(RegAllocStrategy::Greedy);
  ASSERT_NE(RA, nullptr);
  EXPECT_EQ(RA->getStrategy(), RegAllocStrategy::Greedy);
  EXPECT_EQ(RA->getName(), "greedy");
}

// ============================================================
// T2: RegAllocFactory creates Basic allocator
// ============================================================

TEST(RegisterAllocatorTest, CreateBasicAllocator) {
  auto RB = RegAllocFactory::create(RegAllocStrategy::Basic);
  ASSERT_NE(RB, nullptr);
  EXPECT_EQ(RB->getStrategy(), RegAllocStrategy::Basic);
  EXPECT_EQ(RB->getName(), "basic");
}

// ============================================================
// T3: TargetFunction default construction
// ============================================================

TEST(RegisterAllocatorTest, TargetFunctionDefault) {
  TargetFunction TF;
  EXPECT_TRUE(TF.getName().empty());
  EXPECT_EQ(TF.getBlocks().size(), 0u);
  EXPECT_EQ(TF.getFrameInfo().StackSize, 0u);
}

// ============================================================
// T4: TargetFunction with name
// ============================================================

TEST(RegisterAllocatorTest, TargetFunctionWithName) {
  TargetFunction TF("main", nullptr);
  EXPECT_EQ(TF.getName(), "main");
  EXPECT_EQ(TF.getSignature(), nullptr);
}

// ============================================================
// T5: TargetRegisterInfo
// ============================================================

TEST(RegisterAllocatorTest, TargetRegisterInfo) {
  TargetRegisterInfo TRI;
  TRI.setNumRegisters(32);
  EXPECT_EQ(TRI.getNumRegisters(), 32u);
}

// ============================================================
// T6: Greedy allocate stub
// ============================================================

TEST(RegisterAllocatorTest, GreedyAllocateStub) {
  TargetFunction F;
  TargetRegisterInfo TRI;
  TRI.setNumRegisters(16);
  auto RA = RegAllocFactory::create(RegAllocStrategy::Greedy);
  ASSERT_NE(RA, nullptr);
  EXPECT_TRUE(RA->allocate(F, TRI));
}

// ============================================================
// T7: TargetBasicBlock
// ============================================================

TEST(RegisterAllocatorTest, TargetBasicBlock) {
  TargetBasicBlock BB("entry");
  EXPECT_EQ(BB.getName(), "entry");
  EXPECT_TRUE(BB.instructions().empty());

  auto TI = std::make_unique<TargetInstruction>();
  TI->setMnemonic("mov");
  BB.append(std::move(TI));
  EXPECT_EQ(BB.instructions().size(), 1u);
}

// ============================================================
// T8: TargetRegisterInfo callee/caller saved
// ============================================================

TEST(RegisterAllocatorTest, RegisterInfoCalleeCallerSaved) {
  TargetRegisterInfo TRI;
  TRI.addCalleeSavedReg(4);
  TRI.addCalleeSavedReg(5);
  TRI.addCallerSavedReg(0);
  TRI.addCallerSavedReg(1);

  EXPECT_TRUE(TRI.isCalleeSaved(4));
  EXPECT_TRUE(TRI.isCalleeSaved(5));
  EXPECT_FALSE(TRI.isCalleeSaved(0));
  EXPECT_TRUE(TRI.isCallerSaved(0));
  EXPECT_TRUE(TRI.isCallerSaved(1));
  EXPECT_FALSE(TRI.isCallerSaved(4));
}
