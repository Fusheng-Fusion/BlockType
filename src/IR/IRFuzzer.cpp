#include "blocktype/IR/IRFuzzer.h"

#include <algorithm>

namespace blocktype {
namespace ir {

// ============================================================================
// deltaDebug — 经典 DD 算法最小化触发输入
// ============================================================================

std::vector<uint8_t> IRFuzzer::deltaDebug(ir::ArrayRef<uint8_t> TriggeringInput,
                                            OracleFn Oracle) {
  if (TriggeringInput.empty()) return {};

  std::vector<uint8_t> Input(TriggeringInput.begin(), TriggeringInput.end());

  size_t N = 2;
  while (Input.size() >= 2) {
    size_t ChunkSize = Input.size() / N;
    if (ChunkSize == 0) ChunkSize = 1;

    bool Found = false;
    for (size_t I = 0; I < N && !Found; ++I) {
      size_t Start = I * ChunkSize;
      size_t End = (I == N - 1) ? Input.size() : Start + ChunkSize;

      // 构造补集：移除 [Start, End) 区间
      std::vector<uint8_t> Complement;
      Complement.reserve(Input.size() - (End - Start));
      Complement.insert(Complement.end(), Input.begin(), Input.begin() + Start);
      Complement.insert(Complement.end(), Input.begin() + End, Input.end());

      if (Oracle(ir::ArrayRef<uint8_t>(Complement.data(), Complement.size()))) {
        Input = std::move(Complement);
        N = std::max(N - 1, size_t(2));
        Found = true;
      }
    }

    if (!Found) {
      if (N == Input.size()) break;
      N = std::min(N * 2, Input.size());
    }
  }

  return Input;
}

// ============================================================================
// generateRandomInput — 基于 Seed_ 的确定性伪随机输入生成（LCG）
// ============================================================================

std::vector<uint8_t> IRFuzzer::generateRandomInput(size_t Size) {
  std::vector<uint8_t> Result(Size);
  unsigned S = Seed_;
  for (size_t I = 0; I < Size; ++I) {
    // 线性同余生成器 (glibc 常量)
    S = S * 1103515245u + 12345u;
    Result[I] = static_cast<uint8_t>(S >> 16);
  }
  return Result;
}

// ============================================================================
// mutate — 确定性字节级变异（三种策略：位翻转、字节替换、删除子序列）
// ============================================================================

/// LCG 辅助函数，推进伪随机状态并返回下一个值。
static unsigned nextRand(unsigned& S) {
  S = S * 1103515245u + 12345u;
  return S;
}

std::vector<uint8_t> IRFuzzer::mutate(ir::ArrayRef<uint8_t> Input, unsigned Seed) {
  if (Input.empty()) {
    return {static_cast<uint8_t>(Seed & 0xFF)};
  }

  unsigned S = Seed;

  // 策略选择：0=位翻转，1=字节替换，2=删除子序列
  unsigned Strategy = nextRand(S) % 3;

  if (Strategy == 0) {
    // 策略 A：随机位翻转
    std::vector<uint8_t> Result(Input.begin(), Input.end());
    for (size_t I = 0; I < Result.size(); ++I) {
      unsigned R = nextRand(S);
      Result[I] ^= static_cast<uint8_t>(R >> 16);
    }
    return Result;
  } else if (Strategy == 1) {
    // 策略 B：替换随机字节（直接赋新随机值）
    std::vector<uint8_t> Result(Input.begin(), Input.end());
    for (size_t I = 0; I < Result.size(); ++I) {
      unsigned R = nextRand(S);
      Result[I] = static_cast<uint8_t>(R >> 16);
    }
    return Result;
  } else {
    // 策略 C：删除随机子序列
    size_t Len = Input.size();
    unsigned R1 = nextRand(S);
    unsigned R2 = nextRand(S);
    size_t DelStart = static_cast<size_t>(R1) % Len;
    size_t DelLen = static_cast<size_t>(R2) % (Len - DelStart);
    if (DelLen == 0) DelLen = 1;

    std::vector<uint8_t> Result;
    Result.reserve(Len - DelLen);
    Result.insert(Result.end(), Input.begin(), Input.begin() + DelStart);
    Result.insert(Result.end(), Input.begin() + DelStart + DelLen, Input.end());
    return Result;
  }
}

} // namespace ir
} // namespace blocktype
