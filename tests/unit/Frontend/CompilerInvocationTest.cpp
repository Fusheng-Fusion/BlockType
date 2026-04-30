//===--- CompilerInvocationTest.cpp - CompilerInvocation tests -*- C++ -*-===//

#include <string>

#include <gtest/gtest.h>

#include "blocktype/Frontend/CompilerInvocation.h"

using namespace blocktype;

// ============================================================
// T1: Default values
// ============================================================

TEST(CompilerInvocationPipelineTest, DefaultValues) {
  CompilerInvocation CI;
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
}

// ============================================================
// T2: Explicit set via setters
// ============================================================

TEST(CompilerInvocationPipelineTest, SetViaSetters) {
  CompilerInvocation CI;
  CI.setFrontendName("bt");
  CI.setBackendName("cranelift");
  EXPECT_EQ(CI.getFrontendName(), "bt");
  EXPECT_EQ(CI.getBackendName(), "cranelift");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
}

// ============================================================
// T3: Command-line parsing --frontend/--backend
// ============================================================

TEST(CompilerInvocationPipelineTest, ParseCommandLine) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "--frontend=bt", "--backend=cranelift", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(4, Args));
  EXPECT_EQ(CI.getFrontendName(), "bt");
  EXPECT_EQ(CI.getBackendName(), "cranelift");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  EXPECT_EQ(CI.FrontendOpts.InputFiles.size(), 1u);
  EXPECT_EQ(CI.FrontendOpts.InputFiles[0], "test.cpp");
}

// ============================================================
// T4: Command-line parsing without --frontend/--backend
// ============================================================

TEST(CompilerInvocationPipelineTest, ParseCommandLineDefaults) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(2, Args));
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
}

// ============================================================
// T5: toString includes pipeline options
// ============================================================

TEST(CompilerInvocationPipelineTest, ToStringIncludesPipeline) {
  CompilerInvocation CI;
  CI.setFrontendName("bt");
  CI.setBackendName("cranelift");
  std::string S = CI.toString();
  EXPECT_NE(S.find("Frontend Name: bt"), std::string::npos);
  EXPECT_NE(S.find("Backend Name: cranelift"), std::string::npos);
  EXPECT_NE(S.find("Frontend Explicitly Set: yes"), std::string::npos);
  EXPECT_NE(S.find("Backend Explicitly Set: yes"), std::string::npos);
}

// ============================================================
// T6: Validate rejects empty names
// ============================================================

TEST(CompilerInvocationPipelineTest, ValidateRejectsEmptyFrontend) {
  CompilerInvocation CI;
  CI.FrontendName = "";
  EXPECT_FALSE(CI.validate());
}

TEST(CompilerInvocationPipelineTest, ValidateRejectsEmptyBackend) {
  CompilerInvocation CI;
  CI.BackendName = "";
  EXPECT_FALSE(CI.validate());
}

// ============================================================
// T7: --use-new-pipeline flag
// ============================================================

TEST(CompilerInvocationPipelineTest, UseNewPipelineFlag) {
  CompilerInvocation CI;
  const char* Args[] = {"blocktype", "--use-new-pipeline", "test.cpp"};
  EXPECT_TRUE(CI.parseCommandLine(3, Args));
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
  // Names keep defaults
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
}
