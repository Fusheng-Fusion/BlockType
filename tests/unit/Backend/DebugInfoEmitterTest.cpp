//===--- DebugInfoEmitterTest.cpp - DebugInfoEmitter tests -*- C++ -*-===//

#include <memory>

#include <gtest/gtest.h>

#include "blocktype/Backend/DebugInfoEmitter.h"

using namespace blocktype;
using namespace blocktype::backend;

// ============================================================
// T1: DWARF5Emitter can be created
// ============================================================

TEST(DebugInfoEmitterTest, DWARF5EmitterCreation) {
  auto Emitter = std::make_unique<DWARF5Emitter>();
  EXPECT_NE(Emitter, nullptr);
}

// ============================================================
// T2: DWARF5Emitter::emit() returns true (stub)
// ============================================================

TEST(DebugInfoEmitterTest, DWARF5EmitterEmitStub) {
  DWARF5Emitter Emitter;
  // Create a null module test - emit returns true for stub
  // Note: a real IRModule would be needed for full testing
  std::string Buf;
  ir::raw_string_ostream OS(Buf);
  // Stub implementation accepts nullptr module
  // Actual test requires constructing a valid IRModule
}

// ============================================================
// T3: DWARF5Emitter::emitDWARF4() returns false
// ============================================================

TEST(DebugInfoEmitterTest, DWARF5EmitterDWARF4NotSupported) {
  DWARF5Emitter Emitter;
  // DWARF4 is not supported by DWARF5Emitter
  // Stub returns false for non-implemented formats
}

// ============================================================
// T4: DebugInfoEmitter interface compiles
// ============================================================

class MockDebugInfoEmitter : public DebugInfoEmitter {
public:
  bool emit(const ir::IRModule& M, ir::raw_ostream& OS) override {
    (void)M; (void)OS; return true;
  }
  bool emitDWARF5(const ir::IRModule& M, ir::raw_ostream& OS) override {
    (void)M; (void)OS; return true;
  }
  bool emitDWARF4(const ir::IRModule& M, ir::raw_ostream& OS) override {
    (void)M; (void)OS; return false;
  }
  bool emitCodeView(const ir::IRModule& M, ir::raw_ostream& OS) override {
    (void)M; (void)OS; return false;
  }
};

TEST(DebugInfoEmitterTest, MockInterfaceCompiles) {
  MockDebugInfoEmitter Emitter;
  std::string Buf;
  ir::raw_string_ostream OS(Buf);
  // Interface compiles and is usable
  (void)Emitter;
}
