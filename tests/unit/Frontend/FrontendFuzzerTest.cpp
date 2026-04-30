#include <gtest/gtest.h>
#include "blocktype/Testing/FrontendFuzzer.h"

using namespace blocktype::testing;

TEST(FrontendFuzzerTest, GenerateRandomSource) {
  FrontendFuzzer Fuzzer;
  std::string Src = Fuzzer.generateRandomSource(42);
  EXPECT_FALSE(Src.empty());
}

TEST(FrontendFuzzerTest, GenerateWithDifferentSeeds) {
  FrontendFuzzer Fuzzer;
  auto A = Fuzzer.generateRandomSource(1);
  auto B = Fuzzer.generateRandomSource(2);
  // 不同种子产生不同结果（极大概率）
  EXPECT_NE(A, B);
}

TEST(FrontendFuzzerTest, MutateSource) {
  FrontendFuzzer Fuzzer;
  std::string Src = "int main() { return 0; }";
  auto Mutated = Fuzzer.mutateSource(Src, 123);
  EXPECT_FALSE(Mutated.empty());
}

TEST(FrontendFuzzerTest, MutateEmptySource) {
  FrontendFuzzer Fuzzer;
  auto Result = Fuzzer.mutateSource("", 42);
  EXPECT_FALSE(Result.empty()); // 空输入生成随机代码
}

TEST(FrontendFuzzerTest, FuzzIterations) {
  FrontendFuzzer Fuzzer;
  auto Result = Fuzzer.fuzz(10, 42);
  EXPECT_EQ(Result.IterationsRun, 10u);
}

TEST(FrontendFuzzerTest, FuzzFromSeed) {
  FrontendFuzzer Fuzzer;
  auto Result = Fuzzer.fuzzFromSeed("int x;", 5);
  EXPECT_EQ(Result.IterationsRun, 5u);
}

TEST(FrontendFuzzerTest, MinimizeTrigger) {
  FrontendFuzzer Fuzzer;
  auto Min = Fuzzer.minimizeTrigger("int x = 1;");
  EXPECT_FALSE(Min.empty());
}
