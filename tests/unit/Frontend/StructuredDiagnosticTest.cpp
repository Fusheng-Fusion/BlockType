#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "blocktype/Frontend/StructuredDiagnostic.h"

using namespace blocktype;
using namespace blocktype::diag;

// ============================================================
// 测试：DetailedStructuredDiagnostic 创建和字段赋值
// ============================================================

TEST(StructuredDiagnosticTest, CreateDetailedDiag) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Message = "Cannot map QualType to IRType";
  D.Category = "type-error";
  D.FlagName = "-Wir";
  D.IRRelatedDialect = ir::dialect::DialectID::Cpp;

  EXPECT_EQ(D.Level, DiagnosticLevel::Error);
  EXPECT_EQ(D.Message, "Cannot map QualType to IRType");
  EXPECT_EQ(D.Category, "type-error");
  EXPECT_EQ(D.FlagName, "-Wir");
  EXPECT_TRUE(D.IRRelatedDialect.has_value());
  EXPECT_EQ(*D.IRRelatedDialect, ir::dialect::DialectID::Cpp);
  EXPECT_FALSE(D.IRRelatedOpcode.has_value());
}

// ============================================================
// 测试：FixItHint
// ============================================================

TEST(StructuredDiagnosticTest, FixItHint) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Warning;
  D.Message = "Use int instead";

  FixItHint FI;
  FI.Replacement = "int";
  FI.Description = "Replace with int";
  D.FixIts.push_back(FI);

  ASSERT_EQ(D.FixIts.size(), 1u);
  EXPECT_EQ(D.FixIts[0].Replacement, "int");
  EXPECT_EQ(D.FixIts[0].Description, "Replace with int");
}

// ============================================================
// 测试：DiagnosticNote
// ============================================================

TEST(StructuredDiagnosticTest, DiagnosticNote) {
  DetailedStructuredDiagnostic D;
  DiagnosticNote N;
  N.Message = "See declaration here";
  D.DetailedNotes.push_back(N);

  ASSERT_EQ(D.DetailedNotes.size(), 1u);
  EXPECT_EQ(D.DetailedNotes[0].Message, "See declaration here");
}

// ============================================================
// 测试：DetailedDiagEmitter emit + JSON 输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitJSON) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Code = DiagnosticCode::TypeMappingFailed;
  D.Group = DiagnosticGroup::TypeMapping;
  D.Message = "Cannot map QualType to IRType";
  D.Category = "type-error";
  D.FlagName = "-Wir";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string JSON;
  ir::raw_string_ostream OS(JSON);
  Emitter.emitJSON(OS);

  EXPECT_NE(JSON.find("type-error"), std::string::npos);
  EXPECT_NE(JSON.find("Cannot map QualType to IRType"), std::string::npos);
  EXPECT_NE(JSON.find("\"flag\": \"-Wir\""), std::string::npos);
}

// ============================================================
// 测试：SARIF 输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitSARIF) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Code = DiagnosticCode::TypeMappingFailed;
  D.Message = "test error";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string SARIF;
  ir::raw_string_ostream OS(SARIF);
  Emitter.emitSARIF(OS);

  EXPECT_NE(SARIF.find("sarif-schema"), std::string::npos);
  EXPECT_NE(SARIF.find("test error"), std::string::npos);
}

// ============================================================
// 测试：文本输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitText) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Warning;
  D.Code = DiagnosticCode::TypeMappingAmbiguous;
  D.Message = "ambiguous type";
  D.Category = "type-warn";
  D.FlagName = "-Wtype";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string Text;
  ir::raw_string_ostream OS(Text);
  Emitter.emitText(OS);

  EXPECT_NE(Text.find("ambiguous type"), std::string::npos);
  EXPECT_NE(Text.find("type-warn"), std::string::npos);
  EXPECT_NE(Text.find("-Wtype"), std::string::npos);
}

// ============================================================
// 测试：基类 StructuredDiagnostic 向下兼容
// ============================================================

TEST(StructuredDiagnosticTest, BaseClassCompatibility) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Fatal;
  D.Code = DiagnosticCode::VerificationFailed;
  D.Group = DiagnosticGroup::IRVerification;
  D.Message = "fatal verification failure";

  // 基类方法仍然可用
  EXPECT_EQ(D.getLevel(), DiagnosticLevel::Fatal);
  EXPECT_EQ(D.getCode(), DiagnosticCode::VerificationFailed);
  EXPECT_EQ(D.getGroup(), DiagnosticGroup::IRVerification);
}

// ============================================================
// 测试：DiagnosticLevel 枚举值
// ============================================================

TEST(StructuredDiagnosticTest, DiagnosticLevelValues) {
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Note), 0);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Remark), 1);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Warning), 2);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Error), 3);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Fatal), 4);
}
