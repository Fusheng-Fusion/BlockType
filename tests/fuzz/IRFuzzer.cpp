#include "blocktype/IR/IRFuzzer.h"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace blocktype;

// ============================================================================
// F-12 IRFuzzer 验收测试
// ============================================================================

static void testV1_DeltaDebug() {
  // V1: delta debugging 最小化
  auto TriggeringInput = std::vector<uint8_t>(100, 0x42);
  ir::OracleFn Oracle = [](ir::ArrayRef<uint8_t> Input) -> bool {
    return Input.size() >= 3
           && Input[0] == 0x42 && Input[1] == 0x42 && Input[2] == 0x42;
  };
  auto Minimized = ir::IRFuzzer::deltaDebug(
      ir::ArrayRef<uint8_t>(TriggeringInput.data(), TriggeringInput.size()), Oracle);
  assert(Minimized.size() <= TriggeringInput.size() / 2);
  assert(Oracle(ir::ArrayRef<uint8_t>(Minimized.data(), Minimized.size())) == true);
  std::cout << "[PASS] V1: deltaDebug minimized " << TriggeringInput.size()
            << " -> " << Minimized.size() << " bytes\n";
}

static void testV2_DeterministicRandom() {
  // V2: 确定性随机输入
  ir::IRFuzzer Fuzzer;
  Fuzzer.setSeed(12345);
  auto RandInput = Fuzzer.generateRandomInput(64);
  assert(RandInput.size() == 64);

  Fuzzer.setSeed(12345);
  auto RandInput2 = Fuzzer.generateRandomInput(64);
  assert(RandInput == RandInput2);
  std::cout << "[PASS] V2: deterministic random input\n";
}

static void testV3_Mutate() {
  // V3: 变异
  ir::IRFuzzer Fuzzer;
  Fuzzer.setSeed(12345);
  auto RandInput = Fuzzer.generateRandomInput(64);
  auto Mutated = ir::IRFuzzer::mutate(
      ir::ArrayRef<uint8_t>(RandInput.data(), RandInput.size()), 999);
  assert(Mutated.size() > 0);
  std::cout << "[PASS] V3: mutate produces output of size " << Mutated.size() << "\n";
}

int main() {
  testV1_DeltaDebug();
  testV2_DeterministicRandom();
  testV3_Mutate();
  std::cout << "[ALL PASSED] IRFuzzer acceptance tests\n";
  return 0;
}
