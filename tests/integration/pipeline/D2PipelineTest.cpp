//===--- D2PipelineTest.cpp - New pipeline integration tests -*- C++ -*-===//

#include <gtest/gtest.h>

#include "blocktype/Frontend/CompilerInvocation.h"
#include "blocktype/Frontend/CompilerInstance.h"
#include "blocktype/Frontend/FrontendRegistry.h"
#include "blocktype/Backend/BackendRegistry.h"
#include "blocktype/Basic/Diagnostics.h"

using namespace blocktype;

// ============================================================
// CompilerInvocation pipeline field tests
// ============================================================

TEST(D2Pipeline, DefaultInvocationValues) {
  CompilerInvocation CI;
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
}

TEST(D2Pipeline, ExplicitSetInvocation) {
  CompilerInvocation CI;
  CI.setFrontendName("bt");
  CI.setBackendName("cranelift");
  EXPECT_EQ(CI.getFrontendName(), "bt");
  EXPECT_EQ(CI.getBackendName(), "cranelift");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
}

TEST(D2Pipeline, ParseCommandLineFrontendBackend) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "--frontend=cpp", "--backend=llvm", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(4, Args));
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
}

TEST(D2Pipeline, FrontendRegistryCreate) {
  using namespace frontend;
  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = FrontendRegistry::instance().create("cpp", Opts, Diags);
  ASSERT_NE(FE, nullptr);
  EXPECT_EQ(FE->getName(), "cpp");
}

TEST(D2Pipeline, BackendRegistryCreate) {
  using namespace backend;
  DiagnosticsEngine Diags;
  BackendOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto BE = BackendRegistry::instance().create("llvm", Opts, Diags);
  ASSERT_NE(BE, nullptr);
  EXPECT_EQ(BE->getName(), "llvm");
}

TEST(D2Pipeline, UseNewPipelineFlag) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "--use-new-pipeline", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(3, Args));
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
}

// ============================================================
// Error path tests
// ============================================================

TEST(D2Pipeline, NonexistentFrontendName) {
  using namespace frontend;
  DiagnosticsEngine Diags;
  FrontendCompileOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto FE = FrontendRegistry::instance().create("nonexistent", Opts, Diags);
  EXPECT_EQ(FE, nullptr);
}

TEST(D2Pipeline, NonexistentBackendName) {
  using namespace backend;
  DiagnosticsEngine Diags;
  BackendOptions Opts;
  Opts.TargetTriple = "x86_64-unknown-linux-gnu";

  auto BE = BackendRegistry::instance().create("nonexistent", Opts, Diags);
  EXPECT_EQ(BE, nullptr);
}
