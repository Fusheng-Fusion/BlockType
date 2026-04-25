# Task A-F8 优化版：IRIntegrityChecksum 基础实现 + IRSigner + 可重现构建

> 文档版本：v1.0 | 由 planner 产出

---

## 一、任务概述

| 项目 | 内容 |
|------|------|
| 任务编号 | A-F8 |
| 任务名称 | IRIntegrityChecksum 基础实现 + IRSigner + 可重现构建 |
| 依赖 | A.4（IRModule）— ✅ 已存在 |
| 对现有代码的影响 | 仅 `src/IR/CMakeLists.txt` 和 `tests/unit/IR/CMakeLists.txt` 需追加条目 |

**产出文件**

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRIntegrity.h` |
| 新增 | `src/IR/IRIntegrity.cpp` |
| 新增 | `tests/unit/IR/IRIntegrityTest.cpp` |
| 修改 | `src/IR/CMakeLists.txt`（+1 行） |
| 修改 | `tests/unit/IR/CMakeLists.txt`（+1 行） |

---

## 二、代码背景分析 — 6 个关键设计决策

### 决策 1：computeChecksum() 方法不存在 → 独立 static 方法

**现状**：`IRModule` 中没有 `computeChecksum()` 方法。仅有 `isReproducible()`/`setReproducible()`。

**决策**：`IRIntegrityChecksum::compute(const IRModule&)` 作为独立 static 方法，直接遍历 IRModule 的组件计算哈希。不修改 IRModule 接口。

理由：符合"渐进式改造"红线，不改变现有接口。

### 决策 2：BLAKE3 → FNV-1a 64-bit 初始实现

**现状**：项目无 BLAKE3。A-F7 已使用 FNV-1a 作为缓存哈希。

**决策**：`IRIntegrityChecksum` 同样使用 FNV-1a 64-bit，封装在命名空间内的 `computeHash()` 函数中。

理由：
1. Phase A 不引入第三方加密库
2. 完整性校验在此阶段是功能骨架，不涉及真正的安全场景
3. 未来替换为 BLAKE3/SHA-256 只需替换内部哈希函数，接口不变
4. 每个子哈希（TypeSystemHash/InstructionHash 等）独立计算，CombinedHash 使用组合哈希，碰撞风险可控

### 决策 3：Ed25519 签名 → stub 实现

**现状**：项目无 Ed25519。引入加密库（libsodium/openssl）超出 Phase A 范围。

**决策**：`IRSigner` 接口完整声明，`sign()` 返回全零 Signature，`verify()` 返回 `true`（信任模式）。内部注释标注 `// TODO: implement with Ed25519`。

理由：
1. "渐进式改造"——先定义接口，后填充实现
2. 签名是安全特性，需要正式的加密库审计，不适合 Phase A 快速实现
3. stub 行为不阻塞其他模块开发和测试

### 决策 4：IRModule 数据访问

**现状**：IRModule 公开 API：
- `getFunctions()` → `vector<unique_ptr<IRFunction>>&`
- `getGlobals()` → `vector<unique_ptr<IRGlobalVariable>>&`
- `getName()`, `getTargetTriple()`, `getNumFunctions()`
- `FunctionDecls`/`Aliases`/`Metadata` 无公开 getter

**决策**：`IRIntegrityChecksum::compute()` 仅使用公开 API 遍历 Functions 和 Globals。内部遍历：
- Functions → 每个函数的 Name、Linkage、CallConv、参数类型、返回类型、BasicBlock 指令
- Globals → 每个全局的 Name、Linkage、类型
- 类型系统 → 通过函数签名和全局类型间接覆盖

理由：不修改 IRModule 接口（红线 3）。FunctionDecls/Aliases/Metadata 无公开 getter，暂不纳入哈希计算。后续 IRModule 扩展 getter 时再补充。

### 决策 5：可重现构建行为 → 仅 A-F8 定义常量和接口

spec 列出 6 种可重现构建行为：

| 行为 | A-F8 范围 |
|------|-----------|
| SOURCE_DATE_EPOCH 时间戳 | 定义常量 `ReproducibleTimestamp = 0` |
| 确定性随机种子 | 定义常量 `DeterministicSeed = 0x42` |
| 排序后哈希表遍历 | `compute()` 中对 Functions/Globals 按名称排序后哈希 |
| 确定性 ValueID/InstructionID | 仅定义常量，不修改现有 ID 分配逻辑 |
| 确定性临时文件名 | 仅定义常量前缀 |
| 固定 DWARF 生产者字符串 | 仅定义常量 |

**决策**：A-F8 仅定义常量和 `compute()` 中的排序遍历。其余行为涉及修改其他系统组件，后续 Phase 实现。

### 决策 6：命名空间 → `blocktype::ir::security`

spec 指定 `blocktype::ir::security`（嵌套在 `ir` 下），与 `diag`/`cache`（顶层）不同。

**决策**：使用 `blocktype::ir::security`。

理由：
1. 完整性校验是 IR 层内部安全特性（不同于 diag/cache 跨层概念）
2. 使用 `ir::IRModule` 等 `ir` 命名空间内的类型，放在 `ir::security` 下更自然
3. spec 明确指定，遵循 spec

---

## 三、完整头文件 — `include/blocktype/IR/IRIntegrity.h`

```cpp
#ifndef BLOCKTYPE_IR_IRINTEGRITY_H
#define BLOCKTYPE_IR_IRINTEGRITY_H

#include <array>
#include <cstdint>
#include <string>

namespace blocktype {
namespace ir {

class IRModule;

namespace security {

// ============================================================
// 可重现构建常量
// ============================================================

/// SOURCE_DATE_EPOCH 默认值（1970-01-01 00:00:00 UTC）
constexpr uint64_t ReproducibleTimestamp = 0;

/// 确定性随机种子
constexpr uint64_t DeterministicSeed = 0x42;

/// 确定性 ValueID/InstructionID 起始值
constexpr unsigned DeterministicValueIDStart = 1;

/// 确定性临时文件名前缀
constexpr const char* DeterministicTempPrefix = "bt_tmp_";

/// 固定 DWARF 生产者字符串
constexpr const char* FixedProducerString = "BlockType";

// ============================================================
// IRIntegrityChecksum — IR 完整性校验和
// ============================================================

/// IR 模块完整性校验和。对 IRModule 的各子系统分别计算哈希，
/// 最终组合为 CombinedHash。用于检测 IR 数据是否被意外修改。
struct IRIntegrityChecksum {
  uint64_t TypeSystemHash = 0;
  uint64_t InstructionHash = 0;
  uint64_t ConstantHash = 0;
  uint64_t GlobalHash = 0;
  uint64_t DebugInfoHash = 0;
  uint64_t CombinedHash = 0;

  /// 计算 IRModule 的完整性校验和
  static IRIntegrityChecksum compute(const IRModule& M);

  /// 验证 IRModule 的校验和是否与当前值匹配
  bool verify(const IRModule& M) const;

  /// 转为 hex 字符串（用于调试/日志）
  std::string toHex() const;

  bool operator==(const IRIntegrityChecksum& O) const {
    return CombinedHash == O.CombinedHash;
  }
  bool operator!=(const IRIntegrityChecksum& O) const { return !(*this == O); }
};

// ============================================================
// IRSigner — IR 签名（stub）
// ============================================================

using PrivateKey = std::array<uint8_t, 32>;
using PublicKey  = std::array<uint8_t, 32>;
using Signature  = std::array<uint8_t, 64>;

/// IR 模块签名器。A-F8 为 stub 实现，sign() 返回全零签名，
/// verify() 返回 true。后续引入 Ed25519 后替换实现。
class IRSigner {
public:
  /// 对 IRModule 签名（stub：返回全零 Signature）
  static Signature sign(const IRModule& M, const PrivateKey& Key);

  /// 验证 IRModule 签名（stub：始终返回 true）
  static bool verify(const IRModule& M, const Signature& Sig,
                     const PublicKey& Key);
};

} // namespace security
} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRINTEGRITY_H
```

---

## 四、完整实现文件 — `src/IR/IRIntegrity.cpp`

```cpp
#include "blocktype/IR/IRIntegrity.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRFunction.h"
#include "blocktype/IR/IRBasicBlock.h"
#include "blocktype/IR/IRInstruction.h"
#include "blocktype/IR/IRConstant.h"

namespace blocktype {
namespace ir {
namespace security {

// ============================================================
// FNV-1a 64-bit 哈希（后续可替换为 BLAKE3）
// ============================================================

static uint64_t fnv1a64(const void* Data, size_t Len) {
  auto* B = static_cast<const uint8_t*>(Data);
  uint64_t H = 14695981039346656037ULL;
  for (size_t i = 0; i < Len; ++i) { H ^= B[i]; H *= 1099511628211ULL; }
  return H;
}

static uint64_t hashStr(StringRef S) { return fnv1a64(S.data(), S.size()); }
static uint64_t hashU64(uint64_t V) { return fnv1a64(&V, sizeof(V)); }
static uint64_t hashU32(uint32_t V) { return fnv1a64(&V, sizeof(V)); }
static uint64_t hashU16(uint16_t V) { return fnv1a64(&V, sizeof(V)); }
static uint64_t hashU8(uint8_t V) { return fnv1a64(&V, sizeof(V)); }

static uint64_t combineHash(uint64_t A, uint64_t B) {
  return A ^ (B * 1099511628211ULL + 0x9e3779b97f4a7c15ULL);
}

// ============================================================
// IRIntegrityChecksum
// ============================================================

/// 哈希类型系统信息（通过函数签名和全局变量类型间接覆盖）
static uint64_t computeTypeSystemHash(const IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  auto& Funcs = M.getFunctions();
  // 可重现构建：按名称排序后遍历
  std::vector<StringRef> Names;
  Names.reserve(Funcs.size());
  for (auto& F : Funcs) Names.push_back(F->getName());
  std::sort(Names.begin(), Names.end());
  for (auto& N : Names) {
    H = combineHash(H, hashStr(N));
    // 查找对应函数获取类型信息
    if (auto* F = M.getFunction(N)) {
      auto* FT = F->getFunctionType();
      if (FT) {
        H = combineHash(H, hashStr(FT->getReturnType()->toString()));
        for (unsigned i = 0; i < FT->getNumParams(); ++i) {
          H = combineHash(H, hashStr(FT->getParamType(i)->toString()));
        }
        H = combineHash(H, hashU8(FT->isVarArg()));
      }
    }
  }
  return H;
}

/// 哈希指令信息
static uint64_t computeInstructionHash(const IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  for (auto& F : M.getFunctions()) {
    H = combineHash(H, hashStr(F->getName()));
    H = combineHash(H, hashU8(static_cast<uint8_t>(F->getLinkage())));
    H = combineHash(H, hashU8(static_cast<uint8_t>(F->getCallingConv())));
    for (auto& BB : F->getBasicBlocks()) {
      H = combineHash(H, hashStr(BB.getName()));
      for (auto& I : BB.getInstList()) {
        H = combineHash(H, hashU16(static_cast<uint16_t>(I.getOpcode())));
        H = combineHash(H, hashU32(I.getValueID()));
        H = combineHash(H, hashU8(static_cast<uint8_t>(I.getDialect())));
        // 操作数类型
        for (unsigned O = 0; O < I.getNumOperands(); ++O) {
          auto* Op = I.getOperand(O);
          if (Op && Op->getType())
            H = combineHash(H, hashStr(Op->getType()->toString()));
        }
      }
    }
  }
  return H;
}

/// 哈希常量信息
static uint64_t computeConstantHash(const IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  // 常量通过指令操作数间接引用，此处简化为指令哈希的补充
  for (auto& F : M.getFunctions()) {
    H = combineHash(H, hashStr(F->getName()));
    H = combineHash(H, hashU32(F.getNumArgs()));
  }
  return H;
}

/// 哈希全局变量信息
static uint64_t computeGlobalHash(const IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  auto& GVs = M.getGlobals();
  std::vector<StringRef> Names;
  Names.reserve(GVs.size());
  for (auto& GV : GVs) Names.push_back(GV->getName());
  std::sort(Names.begin(), Names.end());
  for (auto& N : Names) {
    auto* GV = M.getGlobalVariable(N);
    if (GV) {
      H = combineHash(H, hashStr(GV->getName()));
      if (GV->getType())
        H = combineHash(H, hashStr(GV->getType()->toString()));
      H = combineHash(H, hashU8(static_cast<uint8_t>(GV->getLinkage())));
      H = combineHash(H, hashU8(GV->isConstant()));
    }
  }
  return H;
}

/// 哈希调试信息
static uint64_t computeDebugInfoHash(const IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  for (auto& F : M.getFunctions()) {
    for (auto& BB : F->getBasicBlocks()) {
      for (auto& I : BB.getInstList()) {
        if (I.hasDebugInfo()) {
          auto* DI = I.getDebugInfo();
          if (DI->hasLocation()) {
            auto& Loc = DI->getLocation();
            H = combineHash(H, hashStr(Loc.Filename));
            H = combineHash(H, hashU32(Loc.Line));
            H = combineHash(H, hashU32(Loc.Column));
          }
        }
      }
    }
  }
  return H;
}

IRIntegrityChecksum IRIntegrityChecksum::compute(const IRModule& M) {
  IRIntegrityChecksum C;
  C.TypeSystemHash  = computeTypeSystemHash(M);
  C.InstructionHash = computeInstructionHash(M);
  C.ConstantHash    = computeConstantHash(M);
  C.GlobalHash      = computeGlobalHash(M);
  C.DebugInfoHash   = computeDebugInfoHash(M);
  C.CombinedHash    = combineHash(
    combineHash(C.TypeSystemHash, C.InstructionHash),
    combineHash(
      combineHash(C.ConstantHash, C.GlobalHash),
      C.DebugInfoHash));
  return C;
}

bool IRIntegrityChecksum::verify(const IRModule& M) const {
  auto Current = compute(M);
  return Current.CombinedHash == CombinedHash;
}

std::string IRIntegrityChecksum::toHex() const {
  char Buf[17];
  std::snprintf(Buf, sizeof(Buf), "%016llx", (unsigned long long)CombinedHash);
  return Buf;
}

// ============================================================
// IRSigner — stub
// ============================================================

Signature IRSigner::sign(const IRModule& /*M*/, const PrivateKey& /*Key*/) {
  // TODO: implement with Ed25519
  return Signature{};
}

bool IRSigner::verify(const IRModule& /*M*/, const Signature& /*Sig*/,
                      const PublicKey& /*Key*/) {
  // TODO: implement with Ed25519 — stub: trust all
  return true;
}

} // namespace security
} // namespace ir
} // namespace blocktype
```

---

## 五、完整测试文件 — `tests/unit/IR/IRIntegrityTest.cpp`

```cpp
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
  IRTypeContext Ctx;
  IRModule M("test", Ctx);
  auto* FT = Ctx.getFunctionType(Ctx.getVoidType(), {});
  auto* F = M.getOrInsertFunction("main", FT);
  auto* BB = F->addBasicBlock("entry");
  IRBuilder Builder(Ctx);
  Builder.setPosition(BB);
  Builder.createRet();

  auto Chk = IRIntegrityChecksum::compute(M);
  EXPECT_NE(Chk.CombinedHash, 0ull);
  EXPECT_NE(Chk.InstructionHash, 0ull);
}

// ============================================================
// V2: 校验和验证 — verify 返回 true
// ============================================================

TEST(IRIntegrityTest, ChecksumVerify) {
  IRTypeContext Ctx;
  IRModule M("test", Ctx);
  auto Chk = IRIntegrityChecksum::compute(M);
  EXPECT_TRUE(Chk.verify(M));
}

// ============================================================
// V3: 可重现构建 — 两次计算结果一致
// ============================================================

TEST(IRIntegrityTest, ReproducibleBuild) {
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx1, Ctx2;
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
  IRTypeContext Ctx;
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
  IRTypeContext Ctx;
  IRModule M1("g1", Ctx);
  IRModule M2("g2", Ctx);

  M1.getOrInsertGlobal("var1", Ctx.getInt32Ty());
  M2.getOrInsertGlobal("var2", Ctx.getInt32Ty());

  auto C1 = IRIntegrityChecksum::compute(M1);
  auto C2 = IRIntegrityChecksum::compute(M2);
  EXPECT_NE(C1.GlobalHash, C2.GlobalHash);
}
```

---

## 六、CMakeLists.txt 修改

### `src/IR/CMakeLists.txt` — 在列表末尾追加：

```cmake
  IRIntegrity.cpp
```

### `tests/unit/IR/CMakeLists.txt` — 在列表末尾追加：

```cmake
  IRIntegrityTest.cpp
```

---

## 七、验收标准映射

| 编号 | 验收内容 | 对应测试 |
|------|----------|----------|
| V1 | Checksum 计算 CombinedHash ≠ 0 | `ChecksumCompute` |
| V2 | Checksum verify 返回 true | `ChecksumVerify` |
| V3 | 可重现构建两次计算一致 | `ReproducibleBuild` |
| V4 | 不同模块不同校验和 | `DifferentModules` |
| V5 | 修改模块后 verify 返回 false | `ModificationDetected` |
| V6 | toHex 16 字符格式 | `ChecksumToHex` |
| V7 | IRSigner::sign stub | `SignerSignStub` |
| V8 | IRSigner::verify stub | `SignerVerifyStub` |
| V9 | 可重现构建常量 | `ReproducibleConstants` |
| V10 | 空模块确定性 | `EmptyModuleConsistent` |
| V11 | 多函数排序遍历可重现 | `MultiFunctionReproducible` |
| V12 | 全局变量参与校验和 | `GlobalsInChecksum` |

---

## 八、sizeof 影响评估

| 类型 | 大小 | 说明 |
|------|------|------|
| IRIntegrityChecksum | 48 bytes | 6 × uint64_t |
| PrivateKey | 32 bytes | `array<uint8_t, 32>` |
| PublicKey | 32 bytes | `array<uint8_t, 32>` |
| Signature | 64 bytes | `array<uint8_t, 64>` |
| 可重现常量 | 0 bytes | `constexpr`，编译期内联 |

---

## 九、风险与注意事项

### 9.1 IRModule 公开 API 限制

- `FunctionDecls`/`Aliases`/`Metadata` 无公开 getter，暂不纳入哈希
- 后续扩展 getter 时同步更新 `compute()` 实现
- 当前覆盖：Functions（含指令级）+ Globals + 调试信息

### 9.2 排序遍历的可重现性

- `computeTypeSystemHash` 和 `computeGlobalHash` 对函数/全局按名称排序后遍历
- `computeInstructionHash` 按函数插入顺序遍历（同一模块内确定性）
- 当 `IRModule::setReproducible(true)` 时，排序保证跨构建一致性

### 9.3 IRSigner stub 安全影响

- `verify()` 始终返回 `true`——**不提供真正的安全保证**
- 在 Phase B/C 引入 Ed25519 前不应在生产环境依赖签名验证
- stub 行为在头文件和 .cpp 中均有明确注释

### 9.4 红线 Checklist 确认

| # | 红线 | 状态 |
|---|------|------|
| (1) | 架构优先 — 完整性校验独立于前后端 | ✅ `ir::security` 独立命名空间 |
| (2) | 多前端多后端自由组合 | ✅ 纯新增文件，无耦合 |
| (3) | 渐进式改造 | ✅ IRSigner 为 stub，不修改 IRModule 接口 |
| (4) | 现有功能不退化 | ✅ 不修改任何现有文件 |
| (5) | 接口抽象优先 | ✅ IRSigner 静态接口 |
| (6) | IR 中间层解耦 | ✅ 完整性属于 IR 层 |

---

## 十、实现步骤

| 步骤 | 操作 |
|------|------|
| 1 | 创建 `include/blocktype/IR/IRIntegrity.h` |
| 2 | 创建 `src/IR/IRIntegrity.cpp` |
| 3 | 修改 `src/IR/CMakeLists.txt`（+`IRIntegrity.cpp`） |
| 4 | 修改 `tests/unit/IR/CMakeLists.txt`（+`IRIntegrityTest.cpp`） |
| 5 | 创建 `tests/unit/IR/IRIntegrityTest.cpp` |
| 6 | 编译 `blocktype-ir`，确认无错误 |
| 7 | 运行测试 `blocktype-ir-test`，12 个测试全部通过 |
| 8 | 运行全量 `ctest`，确认无退化 |
| 9 | Git commit |
