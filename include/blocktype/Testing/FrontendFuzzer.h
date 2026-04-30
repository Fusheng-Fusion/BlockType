#ifndef BLOCKTYPE_TESTING_FRONTENDFUZZER_H
#define BLOCKTYPE_TESTING_FRONTENDFUZZER_H

#include <cstdint>
#include <string>
#include <random>
#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace testing {

struct FrontendFuzzResult {
  bool ParseSucceeded = false;
  bool ProducedInvalidAST = false;
  std::string TriggerSource;
  std::string ErrorOutput;
  uint64_t IterationsRun = 0;
};

class FrontendFuzzer {
  std::mt19937_64 RNG_;

  // 代码片段模板
  static constexpr const char* Snippets[] = {
    "int main() { return 0; }\n",
    "class Foo { int x; };\n",
    "template<typename T> T id(T v) { return v; }\n",
    "void f() { throw 42; }\n",
    "struct S : Foo { int y; };\n",
    "auto x = 1 + 2;\n",
    "int a[10];\n",
    "enum E { A, B, C };\n",
    "namespace N { int v; }\n",
    "if (true) {} else {}\n",
  };

public:
  explicit FrontendFuzzer(uint64_t Seed = 0) : RNG_(Seed) {}

  /// 用给定种子生成随机 C++ 源码片段
  std::string generateRandomSource(uint64_t Seed);

  /// 变异给定源码
  std::string mutateSource(ir::StringRef Source, uint64_t Seed);

  /// 运行多轮生成，返回结果统计
  FrontendFuzzResult fuzz(uint64_t MaxIterations, uint64_t Seed);

  /// 从种子源码出发变异测试
  FrontendFuzzResult fuzzFromSeed(ir::StringRef SeedSource, uint64_t MaxIterations);

  /// 最小化触发问题的源码（简化版：逐字符删除尝试）
  std::string minimizeTrigger(ir::StringRef TriggerSource);
};

} // namespace testing
} // namespace blocktype

#endif // BLOCKTYPE_TESTING_FRONTENDFUZZER_H
