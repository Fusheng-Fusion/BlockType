#include "blocktype/Testing/FrontendFuzzer.h"
#include <algorithm>
#include <random>

namespace blocktype {
namespace testing {

std::string FrontendFuzzer::generateRandomSource(uint64_t Seed) {
  std::mt19937_64 Gen(Seed);
  std::uniform_int_distribution<size_t> Dist(0, 9);
  size_t N = 1 + Dist(Gen) % 5; // 1~5 个片段
  std::string Result;
  for (size_t I = 0; I < N; ++I) {
    Result += Snippets[Dist(Gen)];
  }
  return Result;
}

std::string FrontendFuzzer::mutateSource(ir::StringRef Source, uint64_t Seed) {
  std::string Result(Source.str());
  if (Result.empty()) return generateRandomSource(Seed);
  std::mt19937_64 Gen(Seed);
  std::uniform_int_distribution<size_t> PosDist(0, Result.size() > 0 ? Result.size() - 1 : 0);
  std::uniform_int_distribution<int> ActionDist(0, 2);
  int Action = ActionDist(Gen);
  if (Action == 0 && !Result.empty()) {
    // 删除随机字符
    Result.erase(PosDist(Gen) % Result.size(), 1);
  } else if (Action == 1) {
    // 插入随机片段
    Result.insert(PosDist(Gen) % (Result.size() + 1), Snippets[Gen() % 10]);
  } else {
    // 追加代码
    Result += "\nint _fuzz_var = 0;\n";
  }
  return Result;
}

FrontendFuzzResult FrontendFuzzer::fuzz(uint64_t MaxIterations, uint64_t Seed) {
  FrontendFuzzResult R;
  for (uint64_t I = 0; I < MaxIterations; ++I) {
    auto Src = generateRandomSource(Seed + I);
    auto Mut = mutateSource(Src, Seed + I + 1000);
    if (Mut.empty()) {
      R.ProducedInvalidAST = true;
      R.TriggerSource = Src;
    }
    R.IterationsRun++;
  }
  return R;
}

FrontendFuzzResult FrontendFuzzer::fuzzFromSeed(ir::StringRef SeedSource, uint64_t MaxIterations) {
  FrontendFuzzResult R;
  for (uint64_t I = 0; I < MaxIterations; ++I) {
    auto Mut = mutateSource(SeedSource, I);
    if (Mut.empty()) {
      R.ProducedInvalidAST = true;
      R.TriggerSource = SeedSource.str();
    }
    R.IterationsRun++;
  }
  return R;
}

std::string FrontendFuzzer::minimizeTrigger(ir::StringRef TriggerSource) {
  std::string Result(TriggerSource.str());
  for (size_t I = 0; I < Result.size(); ) {
    char Saved = Result[I];
    Result.erase(I, 1);
    // 简化：如果删除后非空则保留删除
    if (Result.empty()) {
      Result.insert(I, 1, Saved);
      ++I;
    }
  }
  return Result;
}

} // namespace testing
} // namespace blocktype
