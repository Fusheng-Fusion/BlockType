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
static uint64_t computeTypeSystemHash(IRModule& M) {
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
static uint64_t computeInstructionHash(IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  for (auto& F : M.getFunctions()) {
    H = combineHash(H, hashStr(F->getName()));
    H = combineHash(H, hashU8(static_cast<uint8_t>(F->getLinkage())));
    H = combineHash(H, hashU8(static_cast<uint8_t>(F->getCallingConv())));
    for (auto& BB : F->getBasicBlocks()) {
      H = combineHash(H, hashStr(BB->getName()));
      for (auto& I : BB->getInstList()) {
        H = combineHash(H, hashU16(static_cast<uint16_t>(I->getOpcode())));
        H = combineHash(H, hashU32(I->getValueID()));
        H = combineHash(H, hashU8(static_cast<uint8_t>(I->getDialect())));
        // 操作数类型
        for (unsigned O = 0; O < I->getNumOperands(); ++O) {
          auto* Op = I->getOperand(O);
          if (Op && Op->getType())
            H = combineHash(H, hashStr(Op->getType()->toString()));
        }
      }
    }
  }
  return H;
}

/// 哈希常量信息
static uint64_t computeConstantHash(IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  // 常量通过指令操作数间接引用，此处简化为指令哈希的补充
  for (auto& F : M.getFunctions()) {
    H = combineHash(H, hashStr(F->getName()));
    H = combineHash(H, hashU32(F->getNumArgs()));
  }
  return H;
}

/// 哈希全局变量信息
static uint64_t computeGlobalHash(IRModule& M) {
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
static uint64_t computeDebugInfoHash(IRModule& M) {
  uint64_t H = 0xcbf29ce484222325ULL;
  for (auto& F : M.getFunctions()) {
    for (auto& BB : F->getBasicBlocks()) {
      for (auto& I : BB->getInstList()) {
        if (I->hasDebugInfo()) {
          auto* DI = I->getDebugInfo();
          if (DI && DI->hasLocation()) {
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

IRIntegrityChecksum IRIntegrityChecksum::compute(IRModule& M) {
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

bool IRIntegrityChecksum::verify(IRModule& M) const {
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
