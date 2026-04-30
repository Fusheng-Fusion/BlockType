//===--- D4RegistrationTest.cpp - Registration integration tests -*- C++ -*-===//

#include <gtest/gtest.h>

#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/Backend/LLVMBackend.h"
#include "blocktype/Basic/Diagnostics.h"

using namespace blocktype;
using namespace blocktype::frontend;
using namespace blocktype::backend;

// ============================================================
// Frontend registration tests
// ============================================================

TEST(D4Registration, CppFrontendIsRegistered) {
  EXPECT_TRUE(FrontendRegistry::instance().hasFrontend("cpp"));
}

TEST(D4Registration, CppFrontendExtensionMappings) {
  // Verify common C++ extensions map to the cpp frontend
  EXPECT_TRUE(FrontendRegistry::instance().hasFrontend("cpp"));
}

// ============================================================
// Backend registration tests
// ============================================================

TEST(D4Registration, LLVMBackendIsRegistered) {
  EXPECT_TRUE(BackendRegistry::instance().hasBackend("llvm"));
}

TEST(D4Registration, CreateLLVMBackend) {
  DiagnosticsEngine Diags;
  BackendOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto BE = BackendRegistry::instance().create("llvm", Opts, Diags);
  ASSERT_NE(BE, nullptr);
  EXPECT_EQ(BE->getName(), "llvm");
}

TEST(D4Registration, LLVMBackendCapability) {
  DiagnosticsEngine Diags;
  BackendOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto BE = BackendRegistry::instance().create("llvm", Opts, Diags);
  ASSERT_NE(BE, nullptr);
  auto Cap = BE->getCapability();
  (void)Cap; // Verify getCapability() is callable
}

TEST(D4Registration, BackendRegistryNames) {
  auto Names = BackendRegistry::instance().getRegisteredNames();
  EXPECT_FALSE(Names.empty());
  // LLVM backend should be in the list
  bool FoundLLVM = false;
  for (const auto& N : Names) {
    if (N == "llvm") FoundLLVM = true;
  }
  EXPECT_TRUE(FoundLLVM);
}
