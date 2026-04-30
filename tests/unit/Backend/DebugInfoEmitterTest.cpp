//===--- DebugInfoEmitterTest.cpp - DebugInfoEmitter tests -*- C++ -*-===//

#include <memory>

#include <gtest/gtest.h>

#include "blocktype/Backend/DebugInfoEmitter.h"
#include "blocktype/IR/IRModule.h"
#include "blocktype/IR/IRTypeContext.h"

using namespace blocktype;
using namespace blocktype::backend;
using namespace blocktype::ir;

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
  IRTypeContext TypeCtx;
  IRModule Mod("test", TypeCtx);
  DWARF5Emitter Emitter;
  std::string Buf;
  raw_string_ostream OS(Buf);
  EXPECT_TRUE(Emitter.emit(Mod, OS));
}

// ============================================================
// T3: DWARF5Emitter::emitDWARF4() returns false
// ============================================================

TEST(DebugInfoEmitterTest, DWARF5EmitterDWARF4NotSupported) {
  IRTypeContext TypeCtx;
  IRModule Mod("test", TypeCtx);
  DWARF5Emitter Emitter;
  std::string Buf;
  raw_string_ostream OS(Buf);
  EXPECT_FALSE(Emitter.emitDWARF4(Mod, OS));
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
