//===--- DiagnosticsTest.cpp - Diagnostics Engine Tests ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Basic/SourceManager.h"
#include "blocktype/Basic/Translation.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "gtest/gtest.h"
#include <sstream>

using namespace blocktype;

namespace {

TEST(DiagnosticsTest, EnglishMessages) {
  DiagnosticsEngine Diags;
  Diags.setLanguage(DiagnosticLanguage::English);
  
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_lex_invalid_char),
               "invalid character in source code");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_lex_unterminated_string),
               "unterminated string literal");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_pp_file_not_found),
               "file not found");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_pp_circular_include),
               "circular inclusion detected");
}

TEST(DiagnosticsTest, ChineseMessages) {
  DiagnosticsEngine Diags;
  Diags.setLanguage(DiagnosticLanguage::Chinese);
  
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_lex_invalid_char, 
                                          DiagnosticLanguage::Chinese),
               "源代码中存在无效字符");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_lex_unterminated_string,
                                          DiagnosticLanguage::Chinese),
               "未终止的字符串字面量");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_pp_file_not_found,
                                          DiagnosticLanguage::Chinese),
               "文件未找到");
  EXPECT_STREQ(Diags.getDiagnosticMessage(DiagID::err_pp_circular_include,
                                          DiagnosticLanguage::Chinese),
               "检测到循环包含");
}

TEST(DiagnosticsTest, DiagnosticLevels) {
  DiagnosticsEngine Diags;
  
  EXPECT_EQ(Diags.getDiagnosticLevel(DiagID::err_lex_invalid_char), 
            DiagLevel::Error);
  EXPECT_EQ(Diags.getDiagnosticLevel(DiagID::warn_pp_include_not_implemented), 
            DiagLevel::Warning);
  EXPECT_EQ(Diags.getDiagnosticLevel(DiagID::note_pp_macro_defined_here), 
            DiagLevel::Note);
}

TEST(DiagnosticsTest, ErrorCounting) {
  std::string Output;
  llvm::raw_string_ostream OS(Output);
  DiagnosticsEngine Diags(OS);
  
  Diags.report(SourceLocation(), DiagLevel::Error, "test error 1");
  Diags.report(SourceLocation(), DiagLevel::Error, "test error 2");
  Diags.report(SourceLocation(), DiagLevel::Warning, "test warning");
  
  EXPECT_EQ(Diags.getNumErrors(), 2u);
  EXPECT_EQ(Diags.getNumWarnings(), 1u);
  EXPECT_TRUE(Diags.hasErrorOccurred());
}

TEST(DiagnosticsTest, SeverityNamesEnglish) {
  DiagnosticsEngine Diags;
  Diags.setLanguage(DiagnosticLanguage::English);
  
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Error), "error");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Warning), "warning");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Note), "note");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Fatal), "fatal error");
}

TEST(DiagnosticsTest, SeverityNamesChinese) {
  DiagnosticsEngine Diags;
  Diags.setLanguage(DiagnosticLanguage::Chinese);
  
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Error), "错误");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Warning), "警告");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Note), "备注");
  EXPECT_STREQ(Diags.getSeverityName(DiagLevel::Fatal), "致命错误");
}

TEST(DiagnosticsTest, ReportWithDiagID) {
  std::string Output;
  llvm::raw_string_ostream OS(Output);
  DiagnosticsEngine Diags(OS);
  Diags.setLanguage(DiagnosticLanguage::English);
  
  Diags.report(SourceLocation(), DiagID::err_lex_invalid_char);
  
  EXPECT_TRUE(Output.find("invalid character in source code") != std::string::npos);
  EXPECT_TRUE(Output.find("error") != std::string::npos);
}

TEST(DiagnosticsTest, ReportWithDiagIDChinese) {
  std::string Output;
  llvm::raw_string_ostream OS(Output);
  DiagnosticsEngine Diags(OS);
  Diags.setLanguage(DiagnosticLanguage::Chinese);
  
  Diags.report(SourceLocation(), DiagID::err_lex_invalid_char);
  
  EXPECT_TRUE(Output.find("源代码中存在无效字符") != std::string::npos);
  EXPECT_TRUE(Output.find("错误") != std::string::npos);
}

TEST(DiagnosticsTest, ReportWithExtraText) {
  std::string Output;
  llvm::raw_string_ostream OS(Output);
  DiagnosticsEngine Diags(OS);
  Diags.setLanguage(DiagnosticLanguage::English);
  
  Diags.report(SourceLocation(), DiagID::err_pp_file_not_found, "myfile.bt");
  
  EXPECT_TRUE(Output.find("file not found") != std::string::npos);
  EXPECT_TRUE(Output.find("myfile.bt") != std::string::npos);
}

TEST(DiagnosticsTest, SourceManagerIntegration) {
  std::string Output;
  llvm::raw_string_ostream OS(Output);
  
  SourceManager SM;
  SourceLocation Loc = SM.createMainFileID("test.bt", "let x = 1\n");
  
  DiagnosticsEngine Diags(OS);
  Diags.setSourceManager(&SM);
  Diags.setLanguage(DiagnosticLanguage::English);
  
  Diags.report(Loc, DiagID::err_lex_invalid_char);
  
  // SourceManager integration should produce location output
  // (currently placeholder implementation, just check output is non-empty)
  EXPECT_FALSE(Output.empty());
  EXPECT_TRUE(Output.find("invalid character in source code") != std::string::npos);
}

TEST(DiagnosticsTest, Reset) {
  DiagnosticsEngine Diags;
  
  Diags.report(SourceLocation(), DiagLevel::Error, "error");
  Diags.report(SourceLocation(), DiagLevel::Warning, "warning");
  
  EXPECT_EQ(Diags.getNumErrors(), 1u);
  EXPECT_EQ(Diags.getNumWarnings(), 1u);
  
  Diags.reset();
  
  EXPECT_EQ(Diags.getNumErrors(), 0u);
  EXPECT_EQ(Diags.getNumWarnings(), 0u);
  EXPECT_FALSE(Diags.hasErrorOccurred());
}

//===----------------------------------------------------------------------===//
// TranslationManager Tests
//===----------------------------------------------------------------------===//

TEST(TranslationManagerTest, LoadEnglishYAML) {
  TranslationManager TM;
  bool Loaded = TM.loadTranslations(Language::English, "diagnostics/en-US.yaml");
  ASSERT_TRUE(Loaded);

  TM.setLanguage(Language::English);

  // Check that translations were loaded
  EXPECT_TRUE(TM.hasTranslation("err_undeclared_var"));
  EXPECT_TRUE(TM.hasTranslation("err_type_mismatch"));
  EXPECT_TRUE(TM.hasTranslation("warn_unused_var"));

  // Check message content
  EXPECT_EQ(TM.translate("err_undeclared_var"), "undeclared variable '%0'");
  EXPECT_EQ(TM.translate("err_type_mismatch"), "type mismatch: expected '%0', got '%1'");
}

TEST(TranslationManagerTest, LoadChineseYAML) {
  TranslationManager TM;
  bool Loaded = TM.loadTranslations(Language::Chinese, "diagnostics/zh-CN.yaml");
  ASSERT_TRUE(Loaded);

  TM.setLanguage(Language::Chinese);

  // Check that translations were loaded
  EXPECT_TRUE(TM.hasTranslation("err_undeclared_var"));
  EXPECT_TRUE(TM.hasTranslation("err_type_mismatch"));
  EXPECT_TRUE(TM.hasTranslation("warn_unused_var"));

  // Check message content (Chinese)
  EXPECT_EQ(TM.translate("err_undeclared_var"), "未声明的变量 '%0'");
  EXPECT_EQ(TM.translate("err_type_mismatch"), "类型不匹配：期望 '%0'，实际 '%1'");
}

TEST(TranslationManagerTest, MissingTranslation) {
  TranslationManager TM;
  TM.setLanguage(Language::English);

  // Check that missing keys return the key itself
  EXPECT_EQ(TM.translate("nonexistent_key"), "nonexistent_key");
  EXPECT_FALSE(TM.hasTranslation("nonexistent_key"));
}

TEST(TranslationManagerTest, InvalidYAMLFile) {
  TranslationManager TM;
  bool Loaded = TM.loadTranslations(Language::English, "nonexistent.yaml");
  EXPECT_FALSE(Loaded);
}

} // anonymous namespace
