//===--- LLVMBackendRegistrationTest.cpp - Registration tests -*- C++ -*-===//

#include <gtest/gtest.h>

#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/Backend/LLVMBackend.h"
#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/Basic/Diagnostics.h"

using namespace blocktype;
using namespace blocktype::backend;
using namespace blocktype::frontend;

// ============================================================
// T1: LLVM backend is registered
// ============================================================

TEST(LLVMBackendRegistrationTest, LLVMBackendIsRegistered) {
  EXPECT_TRUE(BackendRegistry::instance().hasBackend("llvm"));
}

// ============================================================
// T2: Cpp frontend is registered (verify existing registration)
// ============================================================

TEST(LLVMBackendRegistrationTest, CppFrontendIsRegistered) {
  EXPECT_TRUE(FrontendRegistry::instance().hasFrontend("cpp"));
}

// ============================================================
// T3: Can create LLVM backend instance
// ============================================================

TEST(LLVMBackendRegistrationTest, CreateLLVMBackendInstance) {
  DiagnosticsEngine Diags;
  BackendOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto BE = BackendRegistry::instance().create("llvm", Opts, Diags);
  ASSERT_NE(BE, nullptr);
  EXPECT_EQ(BE->getName(), "llvm");
}
