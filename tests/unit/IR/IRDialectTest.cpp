#include <gtest/gtest.h>

#include "blocktype/IR/DialectLoweringPass.h"
#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRDialect.h"
#include "blocktype/IR/IRModule.h"

using namespace blocktype;
using namespace blocktype::ir;
using namespace blocktype::ir::dialect;

// V1: DialectCapability bitmask basic functionality
TEST(IRDialectTest, DialectCapabilityBasic) {
  DialectCapability Cap;
  Cap.declareDialect(DialectID::Core);
  Cap.declareDialect(DialectID::Cpp);

  EXPECT_TRUE(Cap.hasDialect(DialectID::Core));
  EXPECT_TRUE(Cap.hasDialect(DialectID::Cpp));
  EXPECT_FALSE(Cap.hasDialect(DialectID::Debug));
  EXPECT_FALSE(Cap.hasDialect(DialectID::Target));
  EXPECT_FALSE(Cap.hasDialect(DialectID::Metadata));

  uint32_t Required = (1u << 0) | (1u << 1); // Core + Cpp
  EXPECT_TRUE(Cap.supportsAll(Required));

  uint32_t RequiredAll = 0x1F; // all 5
  EXPECT_FALSE(Cap.supportsAll(RequiredAll));
  EXPECT_EQ(Cap.getUnsupported(RequiredAll),
            ((1u << 2) | (1u << 3) | (1u << 4)));

  EXPECT_EQ(Cap.getSupportedMask(), ((1u << 0) | (1u << 1)));
}

// V2: Predefined backend capabilities
TEST(IRDialectTest, BackendDialectCaps) {
  auto LLVM = BackendDialectCaps::LLVM();
  EXPECT_TRUE(LLVM.hasDialect(DialectID::Core));
  EXPECT_TRUE(LLVM.hasDialect(DialectID::Cpp));
  EXPECT_TRUE(LLVM.hasDialect(DialectID::Target));
  EXPECT_TRUE(LLVM.hasDialect(DialectID::Debug));
  EXPECT_TRUE(LLVM.hasDialect(DialectID::Metadata));
  EXPECT_EQ(LLVM.getSupportedMask(), 0x1Fu);

  auto Cran = BackendDialectCaps::Cranelift();
  EXPECT_TRUE(Cran.hasDialect(DialectID::Core));
  EXPECT_TRUE(Cran.hasDialect(DialectID::Debug));
  EXPECT_FALSE(Cran.hasDialect(DialectID::Cpp));
  EXPECT_FALSE(Cran.hasDialect(DialectID::Target));
  EXPECT_FALSE(Cran.hasDialect(DialectID::Metadata));
}

// V3: DialectLoweringPass — Invoke → Call lowering
TEST(IRDialectTest, InvokeLowering) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule Mod("test", Ctx);

  auto* FTy = Ctx.getFunctionType(Ctx.getInt32Ty(), {});
  auto* F = Mod.getOrInsertFunction("test_func", FTy);
  auto* Entry = F->addBasicBlock("entry");
  auto* NormalBB = F->addBasicBlock("normal");
  auto* UnwindBB = F->addBasicBlock("unwind");

  // Create Invoke with Cpp dialect directly
  auto* CalleeRef = IRCtx.create<IRConstantFunctionRef>(F);
  auto* NormalRef = IRCtx.create<IRBasicBlockRef>(NormalBB);
  auto* UnwindRef = IRCtx.create<IRBasicBlockRef>(UnwindBB);

  auto InvokeInst = std::make_unique<IRInstruction>(
    Opcode::Invoke, Ctx.getInt32Ty(), 0, DialectID::Cpp, "invoke_result");
  InvokeInst->addOperand(CalleeRef);
  InvokeInst->addOperand(NormalRef);
  InvokeInst->addOperand(UnwindRef);
  Entry->push_back(std::move(InvokeInst));

  // Add terminators to Normal and Unwind BBs
  IRBuilder Builder(IRCtx);
  Builder.setInsertPoint(NormalBB);
  Builder.createRet(Builder.getInt32(0));
  Builder.setInsertPoint(UnwindBB);
  Builder.createRet(Builder.getInt32(0));

  // Run pass with Cranelift caps (Core + Debug only, no Cpp)
  DialectLoweringPass Pass(BackendDialectCaps::Cranelift());
  bool Changed = Pass.run(Mod);

  EXPECT_TRUE(Changed);
  // Verify the entry BB now has a Call instruction (not Invoke)
  auto& EntryInsts = Entry->getInstList();
  ASSERT_FALSE(EntryInsts.empty());
  auto* FirstInst = EntryInsts.front().get();
  EXPECT_EQ(FirstInst->getOpcode(), Opcode::Call);
  EXPECT_EQ(FirstInst->getDialect(), DialectID::Core);
}

// V4: No lowering when only Core instructions
TEST(IRDialectTest, NoLoweringNeeded) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule Mod("test", Ctx);

  auto* FTy = Ctx.getFunctionType(Ctx.getInt32Ty(), {});
  auto* F = Mod.getOrInsertFunction("core_only", FTy);
  auto* Entry = F->addBasicBlock("entry");

  IRBuilder Builder(IRCtx);
  Builder.setInsertPoint(Entry);
  auto* One = Builder.getInt32(1);
  auto* Two = Builder.getInt32(2);
  Builder.createAdd(One, Two, "sum");
  Builder.createRet(Builder.getInt32(0));

  DialectLoweringPass Pass(BackendDialectCaps::Cranelift());
  bool Changed = Pass.run(Mod);

  EXPECT_FALSE(Changed);
}

// V5: LLVM capability — no lowering for any instruction
TEST(IRDialectTest, LLVMFullCapsNoLowering) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule Mod("test", Ctx);

  auto* FTy = Ctx.getFunctionType(Ctx.getInt32Ty(), {});
  auto* F = Mod.getOrInsertFunction("cpp_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  // Create Invoke with Cpp dialect
  auto* CalleeRef = IRCtx.create<IRConstantFunctionRef>(F);
  auto InvokeInst = std::make_unique<IRInstruction>(
    Opcode::Invoke, Ctx.getInt32Ty(), 0, DialectID::Cpp, "invoke_result");
  InvokeInst->addOperand(CalleeRef);
  Entry->push_back(std::move(InvokeInst));

  // LLVM supports all dialects — should not lower
  DialectLoweringPass Pass(BackendDialectCaps::LLVM());
  bool Changed = Pass.run(Mod);

  EXPECT_FALSE(Changed);
}

// New opcode values verification
TEST(IRDialectTest, NewOpcodeValues) {
  EXPECT_EQ(static_cast<uint16_t>(Opcode::DynamicCast), 192u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::VtableDispatch), 193u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::RTTITypeid), 194u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::TargetIntrinsic), 200u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::MetaInlineAlways), 208u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::MetaInlineNever), 209u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::MetaHot), 210u);
  EXPECT_EQ(static_cast<uint16_t>(Opcode::MetaCold), 211u);
}

// New FunctionAttr values verification
TEST(IRDialectTest, NewFunctionAttrValues) {
  EXPECT_EQ(static_cast<uint32_t>(FunctionAttr::Hot), 1u << 14);
  EXPECT_EQ(static_cast<uint32_t>(FunctionAttr::Cold), 1u << 15);
}

// Metadata lowering — MetaHot/MetaCold add function attributes
TEST(IRDialectTest, MetadataLowering) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule Mod("test", Ctx);

  auto* FTy = Ctx.getFunctionType(Ctx.getVoidType(), {});
  auto* F = Mod.getOrInsertFunction("meta_func", FTy);
  auto* Entry = F->addBasicBlock("entry");

  // Create MetaHot instruction with Metadata dialect
  auto HotInst = std::make_unique<IRInstruction>(
    Opcode::MetaHot, Ctx.getVoidType(), 0, DialectID::Metadata);
  Entry->push_back(std::move(HotInst));

  // Create MetaCold instruction with Metadata dialect
  auto ColdInst = std::make_unique<IRInstruction>(
    Opcode::MetaCold, Ctx.getVoidType(), 0, DialectID::Metadata);
  Entry->push_back(std::move(ColdInst));

  // Add a return so the function is well-formed
  IRBuilder Builder(IRCtx);
  Builder.setInsertPoint(Entry);
  Builder.createRetVoid();

  // Initially no Hot/Cold attrs
  EXPECT_EQ(F->getAttributes() & static_cast<uint32_t>(FunctionAttr::Hot), 0u);
  EXPECT_EQ(F->getAttributes() & static_cast<uint32_t>(FunctionAttr::Cold), 0u);

  // Run with Cranelift caps (no Metadata support)
  DialectLoweringPass Pass(BackendDialectCaps::Cranelift());
  bool Changed = Pass.run(Mod);

  EXPECT_TRUE(Changed);
  // Metadata instructions should be erased; function should have attrs
  EXPECT_NE(F->getAttributes() & static_cast<uint32_t>(FunctionAttr::Hot), 0u);
  EXPECT_NE(F->getAttributes() & static_cast<uint32_t>(FunctionAttr::Cold), 0u);
  // Only Ret should remain in entry block
  EXPECT_EQ(Entry->size(), 1u);
}
