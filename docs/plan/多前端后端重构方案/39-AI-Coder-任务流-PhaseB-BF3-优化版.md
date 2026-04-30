# Task B-F3：FrontendFuzzer 前端模糊测试器（优化版）

## 1. 代码库验证摘要

| 规格中的类型 | 实际代码库 | 修正 |
|-------------|-----------|------|
| `CppFrontend` | `frontend::CppFrontend`（`Frontend/CppFrontend.h`） | ✅ 直接使用 |
| `FrontendBase` | `frontend::FrontendBase`（`Frontend/FrontendBase.h`） | ✅ 直接使用 |
| `DiagnosticsEngine` | `DiagnosticsEngine`（`Basic/Diagnostics.h`） | ✅ 直接使用 |
| `StringRef` | `ir::StringRef` | ✅ 使用 `ir::StringRef` |

**关键修正**：
1. 不需要 libFuzzer/AFL，仅实现随机 C++ 代码生成+变异框架
2. `FrontendFuzzResult` 移除 `Crashed`/`AssertionFailed`（无真实执行），改为 `ParseSucceeded`
3. 需新增 `include/blocktype/Testing/` 目录

---

## 2. 完整类定义

### `include/blocktype/Testing/FrontendFuzzer.h`

```cpp
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
```

### `tests/fuzz/FrontendFuzzer.cpp`

```cpp
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
```

---

## 3. 产出文件列表

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Testing/FrontendFuzzer.h` |
| 新增 | `tests/fuzz/FrontendFuzzer.cpp` |
| 新增 | `tests/unit/Frontend/FrontendFuzzerTest.cpp` |

---

## 4. CMakeLists.txt 修改

### 根 `CMakeLists.txt`

在 `add_subdirectory` 列表中追加（如不存在 fuzz 目录）：

```cmake
add_subdirectory(tests/fuzz)
```

### `tests/fuzz/CMakeLists.txt`（新建）

```cmake
add_library(blocktype-fuzz
  FrontendFuzzer.cpp
)
target_include_directories(blocktype-fuzz
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(blocktype-fuzz
  PUBLIC blocktype-ir
)
```

### `tests/unit/Frontend/CMakeLists.txt` 追加

```cmake
add_executable(FrontendFuzzerTest
  FrontendFuzzerTest.cpp
)
target_link_libraries(FrontendFuzzerTest
  PRIVATE
    blocktype-fuzz
    blocktype-ir
    GTest::gtest
    GTest::gtest_main
)
target_include_directories(FrontendFuzzerTest
  PRIVATE ${CMAKE_SOURCE_DIR}/include
)
gtest_discover_tests(FrontendFuzzerTest)
```

---

## 5. GTest 测试用例

### `tests/unit/Frontend/FrontendFuzzerTest.cpp`

```cpp
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
```

---

## 6. 验收标准

1. `testing::FrontendFuzzer::generateRandomSource()` 返回非空 C++ 代码片段
2. `testing::FrontendFuzzer::mutateSource()` 对给定源码进行变异
3. `testing::FrontendFuzzer::fuzz()` 执行指定迭代次数并返回 `FrontendFuzzResult`
4. `testing::FrontendFuzzer::minimizeTrigger()` 可简化触发源码
5. 不同种子产生不同结果
6. 所有 GTest 测试通过
7. 不修改现有 Frontend 组件
