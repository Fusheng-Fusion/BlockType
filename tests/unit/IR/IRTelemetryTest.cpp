#include <gtest/gtest.h>

#include "blocktype/IR/IRTelemetry.h"

#include <fstream>
#include <string>

using namespace blocktype::telemetry;

TEST(TelemetryTest, PhaseGuardRAII) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto Guard = TC.scopePhase(CompilationPhase::Frontend, "test");
  }
  ASSERT_EQ(TC.getEvents().size(), 1u);
  EXPECT_EQ(TC.getEvents()[0].Phase, CompilationPhase::Frontend);
  EXPECT_EQ(TC.getEvents()[0].Detail, "test");
  EXPECT_TRUE(TC.getEvents()[0].Success);
  EXPECT_GT(TC.getEvents()[0].EndNs, TC.getEvents()[0].StartNs);
}

TEST(TelemetryTest, DisabledNoEvents) {
  TelemetryCollector TC;
  // not enabled
  {
    auto Guard = TC.scopePhase(CompilationPhase::Frontend, "test");
  }
  EXPECT_EQ(TC.getEvents().size(), 0u);
}

TEST(TelemetryTest, MarkFailed) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto Guard = TC.scopePhase(CompilationPhase::BackendCodegen, "fail");
    Guard.markFailed();
  }
  ASSERT_EQ(TC.getEvents().size(), 1u);
  EXPECT_FALSE(TC.getEvents()[0].Success);
}

TEST(TelemetryTest, ChromeTraceOutput) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto Guard = TC.scopePhase(CompilationPhase::Frontend, "parse");
  }
  const char* Path = "/tmp/bt_telemetry_trace.json";
  EXPECT_TRUE(TC.writeChromeTrace(Path));

  // Verify the file is valid JSON with expected structure
  std::ifstream IF(Path);
  ASSERT_TRUE(IF.is_open());
  std::string Content((std::istreambuf_iterator<char>(IF)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(Content.find("\"traceEvents\""), std::string::npos);
  EXPECT_NE(Content.find("\"ph\":\"B\""), std::string::npos);
  EXPECT_NE(Content.find("\"ph\":\"E\""), std::string::npos);
  EXPECT_NE(Content.find("\"Frontend\""), std::string::npos);
  EXPECT_NE(Content.find("\"displayTimeUnit\""), std::string::npos);
}

TEST(TelemetryTest, JSONReportOutput) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto Guard = TC.scopePhase(CompilationPhase::IRGeneration, "gen");
  }
  const char* Path = "/tmp/bt_telemetry_report.json";
  EXPECT_TRUE(TC.writeJSONReport(Path));

  std::ifstream IF(Path);
  ASSERT_TRUE(IF.is_open());
  std::string Content((std::istreambuf_iterator<char>(IF)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(Content.find("\"events\""), std::string::npos);
  EXPECT_NE(Content.find("\"summary\""), std::string::npos);
  EXPECT_NE(Content.find("\"IRGeneration\""), std::string::npos);
  EXPECT_NE(Content.find("\"totalDurationNs\""), std::string::npos);
}

TEST(TelemetryTest, MultiplePhases) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto G1 = TC.scopePhase(CompilationPhase::Frontend, "parse");
  }
  {
    auto G2 = TC.scopePhase(CompilationPhase::IRGeneration, "gen");
  }
  ASSERT_EQ(TC.getEvents().size(), 2u);
  EXPECT_EQ(TC.getEvents()[0].Phase, CompilationPhase::Frontend);
  EXPECT_EQ(TC.getEvents()[1].Phase, CompilationPhase::IRGeneration);
}

TEST(TelemetryTest, Clear) {
  TelemetryCollector TC;
  TC.enable();
  {
    auto Guard = TC.scopePhase(CompilationPhase::Frontend, "test");
  }
  EXPECT_EQ(TC.getEvents().size(), 1u);
  TC.clear();
  EXPECT_EQ(TC.getEvents().size(), 0u);
}
