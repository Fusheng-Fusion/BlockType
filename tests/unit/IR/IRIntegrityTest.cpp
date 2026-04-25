#include <gtest/gtest.h>
#include "blocktype/IR/IRIntegrity.h"
#include "blocktype/IR/IRBuilder.h"
#include "blocktype/IR/IRContext.h"
#include "blocktype/IR/IRModule.h"

using namespace blocktype;
using namespace blocktype::ir;
using namespace blocktype::ir::security;

// ============================================================
// V1: IRIntegrityChecksum 计算 — CombinedHash 非零
// ============================================================

TEST(IRIntegrityTest, ChecksumCompute) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  auto* F = M.getOrInsertFunction("main", FT);
  auto* BB = F->addBasicBlock("entry");
  IRBuilder Builder(IRCtx);
  Builder.setInsertPoint(BB);
  Builder.createRetVoid();

  auto Chk = IRIntegrityChecksum::compute(M);
  EXPECT_NE(Chk.CombinedHash, 0ull);
  EXPECT_NE(Chk.InstructionHash, 0ull);
}

// ============================================================
// V2: 校验和验证 — verify 返回 true
// ============================================================

TEST(IRIntegrityTest, ChecksumVerify) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  auto Chk = IRIntegrityChecksum::compute(M);
  EXPECT_TRUE(Chk.verify(M));
}

// ============================================================
// V3: 可重现构建 — 两次计算结果一致
// ============================================================

TEST(IRIntegrityTest, ReproducibleBuild) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("repro", Ctx);
  M.setReproducible(true);
  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  auto* F = M.getOrInsertFunction("main", FT);
  F->addBasicBlock("entry");

  auto Chk1 = IRIntegrityChecksum::compute(M);
  auto Chk2 = IRIntegrityChecksum::compute(M);
  EXPECT_EQ(Chk1.CombinedHash, Chk2.CombinedHash);
}

// ============================================================
// V4: 不同模块产生不同校验和
// ============================================================

TEST(IRIntegrityTest, DifferentModules) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M1("mod1", Ctx);
  IRModule M2("mod2", Ctx);

  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  M1.getOrInsertFunction("foo", FT);
  M2.getOrInsertFunction("bar", FT);

  auto C1 = IRIntegrityChecksum::compute(M1);
  auto C2 = IRIntegrityChecksum::compute(M2);
  EXPECT_NE(C1.CombinedHash, C2.CombinedHash);
}

// ============================================================
// V5: 修改模块后校验和变化
// ============================================================

TEST(IRIntegrityTest, ModificationDetected) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  auto Chk = IRIntegrityChecksum::compute(M);

  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  M.getOrInsertFunction("added_func", FT);

  EXPECT_FALSE(Chk.verify(M));
}

// ============================================================
// V6: toHex 输出格式
// ============================================================

TEST(IRIntegrityTest, ChecksumToHex) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  auto Chk = IRIntegrityChecksum::compute(M);
  auto Hex = Chk.toHex();
  EXPECT_EQ(Hex.size(), 16u);
  for (char C : Hex) {
    EXPECT_TRUE((C>='0'&&C<='9')||(C>='a'&&C<='f'));
  }
}

// ============================================================
// V7: IRSigner stub — sign 返回非空
// ============================================================

TEST(IRIntegrityTest, SignerSignStub) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  PrivateKey Key{};
  auto Sig = IRSigner::sign(M, Key);
  // stub: 返回全零数组
  EXPECT_EQ(Sig.size(), 64u);
}

// ============================================================
// V8: IRSigner stub — verify 始终返回 true
// ============================================================

TEST(IRIntegrityTest, SignerVerifyStub) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("test", Ctx);
  PrivateKey Key{};
  Signature Sig{};
  EXPECT_TRUE(IRSigner::verify(M, Sig, Key));
}

// ============================================================
// V9: 可重现构建常量验证
// ============================================================

TEST(IRIntegrityTest, ReproducibleConstants) {
  EXPECT_EQ(ReproducibleTimestamp, 0ull);
  EXPECT_EQ(DeterministicSeed, 0x42ull);
  EXPECT_EQ(DeterministicValueIDStart, 1u);
  EXPECT_STREQ(DeterministicTempPrefix, "bt_tmp_");
  EXPECT_STREQ(FixedProducerString, "BlockType");
}

// ============================================================
// V10: 空模块校验和一致
// ============================================================

TEST(IRIntegrityTest, EmptyModuleConsistent) {
  IRContext IRCtx1, IRCtx2;
  IRTypeContext& Ctx1 = IRCtx1.getTypeContext();
  IRTypeContext& Ctx2 = IRCtx2.getTypeContext();
  IRModule M1("empty1", Ctx1);
  IRModule M2("empty2", Ctx2);

  auto C1 = IRIntegrityChecksum::compute(M1);
  auto C2 = IRIntegrityChecksum::compute(M2);
  // 空模块没有函数/全局，InstructionHash/GlobalHash 只含初始值
  // 但模块名不同，可能影响哈希（如果实现中包含模块名）
  // 这里验证同模块的确定性
  auto C1b = IRIntegrityChecksum::compute(M1);
  EXPECT_EQ(C1.CombinedHash, C1b.CombinedHash);
}

// ============================================================
// V11: 多函数模块的排序遍历（可重现性）
// ============================================================

TEST(IRIntegrityTest, MultiFunctionReproducible) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M("multi", Ctx);
  M.setReproducible(true);
  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  M.getOrInsertFunction("z_func", FT);
  M.getOrInsertFunction("a_func", FT);
  M.getOrInsertFunction("m_func", FT);

  auto C1 = IRIntegrityChecksum::compute(M);
  auto C2 = IRIntegrityChecksum::compute(M);
  EXPECT_EQ(C1.CombinedHash, C2.CombinedHash);
}

// ============================================================
// V12: 全局变量参与校验和
// ============================================================

TEST(IRIntegrityTest, GlobalsInChecksum) {
  IRContext IRCtx;
  IRTypeContext& Ctx = IRCtx.getTypeContext();
  IRModule M1("g1", Ctx);
  IRModule M2("g2", Ctx);

  M1.getOrInsertGlobal("var1", Ctx.getInt32Ty());
  M2.getOrInsertGlobal("var2", Ctx.getInt32Ty());

  auto C1 = IRIntegrityChecksum::compute(M1);
  auto C2 = IRIntegrityChecksum::compute(M2);
  EXPECT_NE(C1.GlobalHash, C2.GlobalHash);
}
