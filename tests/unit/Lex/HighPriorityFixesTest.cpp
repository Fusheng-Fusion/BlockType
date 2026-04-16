#include <gtest/gtest.h>
#include "blocktype/Lex/Preprocessor.h"
#include "blocktype/Lex/Lexer.h"
#include "blocktype/Basic/SourceManager.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Basic/FileManager.h"
#include "blocktype/Lex/HeaderSearch.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

using namespace blocktype;

class HighPriorityFixesTest : public ::testing::Test {
protected:
  SourceManager SM;
  std::unique_ptr<DiagnosticsEngine> Diags;
  std::unique_ptr<FileManager> FileMgr;
  std::unique_ptr<HeaderSearch> Headers;
  std::unique_ptr<Preprocessor> PP;
  std::string OutputStr;
  std::unique_ptr<llvm::raw_string_ostream> OutputStream;

  void SetUp() override {
    OutputStream = std::make_unique<llvm::raw_string_ostream>(OutputStr);
    Diags = std::make_unique<DiagnosticsEngine>(*OutputStream);
    FileMgr = std::make_unique<FileManager>();
    Headers = std::make_unique<HeaderSearch>(*FileMgr);
    PP = std::make_unique<Preprocessor>(SM, *Diags, Headers.get(), nullptr, FileMgr.get());
  }
};

// Test 1: Recursive macro expansion prevention
TEST_F(HighPriorityFixesTest, RecursiveMacroExpansion) {
  PP->enterSourceFile("test.cpp", "#define FOO FOO\nFOO\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  EXPECT_EQ(Tok.getKind(), TokenKind::identifier);
  EXPECT_EQ(Tok.getText(), "FOO");
}

// Test 2: __FILE__ macro
TEST_F(HighPriorityFixesTest, FileMacro) {
  PP->enterSourceFile("myfile.cpp", "__FILE__\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  EXPECT_EQ(Tok.getKind(), TokenKind::string_literal);
  EXPECT_TRUE(Tok.getText().contains("myfile.cpp"));
}

// Test 3: __LINE__ macro
TEST_F(HighPriorityFixesTest, LineMacro) {
  PP->enterSourceFile("test.cpp", "__LINE__\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  EXPECT_EQ(Tok.getKind(), TokenKind::numeric_constant);
  // Line number should be 1 (or some valid number)
  EXPECT_TRUE(Tok.getText().size() > 0);
}

// Test 4: Circular include detection
TEST_F(HighPriorityFixesTest, CircularIncludeDetection) {
  // Create a file that includes itself
  PP->enterSourceFile("circular.cpp", "#include \"circular.cpp\"\nint x;\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  // Should continue parsing after circular include error
  EXPECT_EQ(Tok.getKind(), TokenKind::kw_int);
  // Should get an error about circular inclusion
  EXPECT_TRUE(Diags->hasErrorOccurred());
}

// Test 5: Nested macro expansion prevention
TEST_F(HighPriorityFixesTest, NestedRecursiveExpansion) {
  PP->enterSourceFile("test.cpp", 
    "#define A B\n"
    "#define B A\n"
    "A\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  // Should expand to B, but not recursively expand B to A
  EXPECT_EQ(Tok.getKind(), TokenKind::identifier);
  EXPECT_EQ(Tok.getText(), "B");
}

// Test 6: Macro with __FILE__ and __LINE__
TEST_F(HighPriorityFixesTest, MacroWithFileAndLine) {
  PP->enterSourceFile("test.cpp", 
    "#define LOG __FILE__ \":\" __LINE__\n"
    "LOG\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  EXPECT_EQ(Tok.getKind(), TokenKind::string_literal);
  EXPECT_TRUE(Tok.getText().contains("test.cpp"));
}

// Test 7: Non-recursive expansion in function-like macro
TEST_F(HighPriorityFixesTest, FunctionLikeRecursiveExpansion) {
  PP->enterSourceFile("test.cpp", 
    "#define FOO(x) FOO(x)\n"
    "FOO(1)\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  // Should not infinitely expand
  EXPECT_EQ(Tok.getKind(), TokenKind::identifier);
}

// Test 8: Multiple __LINE__ calls
TEST_F(HighPriorityFixesTest, MultipleLineMacros) {
  PP->enterSourceFile("test.cpp", 
    "__LINE__\n"
    "__LINE__\n"
    "__LINE__\n");
  
  Token Tok1, Tok2, Tok3;
  ASSERT_TRUE(PP->lexToken(Tok1));
  ASSERT_TRUE(PP->lexToken(Tok2));
  ASSERT_TRUE(PP->lexToken(Tok3));
  
  EXPECT_EQ(Tok1.getKind(), TokenKind::numeric_constant);
  EXPECT_EQ(Tok2.getKind(), TokenKind::numeric_constant);
  EXPECT_EQ(Tok3.getKind(), TokenKind::numeric_constant);
}

// Test 9: FileManager encoding detection
TEST_F(HighPriorityFixesTest, EncodingDetection) {
  // UTF-8 BOM
  char utf8Bom[] = {(char)0xEF, (char)0xBB, (char)0xBF, 't', 'e', 's', 't'};
  EXPECT_EQ(FileManager::detectEncoding(utf8Bom, 7), 
            FileManager::Encoding::UTF8);
  
  // UTF-16 LE BOM
  char utf16LeBom[] = {(char)0xFF, (char)0xFE, 't', 'e'};
  EXPECT_EQ(FileManager::detectEncoding(utf16LeBom, 4), 
            FileManager::Encoding::UTF16_LE);
  
  // UTF-16 BE BOM
  char utf16BeBom[] = {(char)0xFE, (char)0xFF, 't', 'e'};
  EXPECT_EQ(FileManager::detectEncoding(utf16BeBom, 4), 
            FileManager::Encoding::UTF16_BE);
}

// Test 10: Diagnostic system with DiagID
TEST_F(HighPriorityFixesTest, DiagnosticSystem) {
  Diags->report(SourceLocation(), DiagID::err_lex_invalid_char);
  EXPECT_TRUE(Diags->hasErrorOccurred());
  EXPECT_TRUE(OutputStr.find("invalid character") != std::string::npos);
}

// Test 11: Diagnostic with extra text
TEST_F(HighPriorityFixesTest, DiagnosticWithExtraText) {
  Diags->report(SourceLocation(), DiagID::err_pp_file_not_found, "missing.h");
  EXPECT_TRUE(Diags->hasErrorOccurred());
  EXPECT_TRUE(OutputStr.find("missing.h") != std::string::npos);
}

// Test 12: Macro expansion with multiple levels
TEST_F(HighPriorityFixesTest, MultiLevelMacroExpansion) {
  PP->enterSourceFile("test.cpp", 
    "#define A 1\n"
    "#define B A\n"
    "#define C B\n"
    "C\n");
  
  Token Tok;
  ASSERT_TRUE(PP->lexToken(Tok));
  EXPECT_EQ(Tok.getKind(), TokenKind::numeric_constant);
  EXPECT_EQ(Tok.getText(), "1");
}
