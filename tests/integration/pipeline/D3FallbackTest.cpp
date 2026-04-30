//===--- D3FallbackTest.cpp - Fallback pipeline tests -*- C++ -*-===//

#include <gtest/gtest.h>

#include "blocktype/Frontend/CompilerInvocation.h"
#include "blocktype/Frontend/CompilerInstance.h"

using namespace blocktype;

// ============================================================
// Fallback routing logic tests
// ============================================================

TEST(D3Fallback, DefaultInvocationUsesOldPipeline) {
  // When neither --frontend nor --backend is specified,
  // isFrontendExplicitlySet() and isBackendExplicitlySet() are false,
  // so compileFile() should route to runOldPipeline().
  CompilerInvocation CI;
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
  // Routing decision: should use old pipeline
}

TEST(D3Fallback, ExplicitFrontendUsesNewPipeline) {
  // When only --frontend is specified, new pipeline is used
  CompilerInvocation CI;
  CI.setFrontendName("cpp");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  // Routing decision: should use new pipeline
}

TEST(D3Fallback, ExplicitBackendUsesNewPipeline) {
  // When only --backend is specified, new pipeline is used
  CompilerInvocation CI;
  CI.setBackendName("llvm");
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  // Routing decision: should use new pipeline
}

TEST(D3Fallback, BothExplicitUsesNewPipeline) {
  // When both are specified, new pipeline is used
  CompilerInvocation CI;
  CI.setFrontendName("cpp");
  CI.setBackendName("llvm");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  // Routing decision: should use new pipeline
}

TEST(D3Fallback, UseNewPipelineFlagForcesNewPipeline) {
  // --use-new-pipeline forces both ExplicitlySet flags
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "--use-new-pipeline", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(3, Args));
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  // Routing decision: should use new pipeline
}

TEST(D3Fallback, ParseCommandLineWithoutFlagsUsesOldPipeline) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(2, Args));
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
  // Routing decision: should use old pipeline
}
