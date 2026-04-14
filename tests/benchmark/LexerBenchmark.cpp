//===--- LexerBenchmark.cpp - Lexer Performance Benchmark ------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Performance benchmark for the BlockType lexer.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Lex/Lexer.h"
#include "blocktype/Basic/SourceManager.h"
#include "blocktype/Basic/Diagnostics.h"
#include "llvm/Support/raw_ostream.h"
#include <chrono>
#include <string>
#include <random>

using namespace blocktype;

// Generate random identifier
std::string generateIdentifier(size_t length) {
  static const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
  
  std::string result;
  result.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    result += chars[dis(gen)];
  }
  return result;
}

// Generate test source code
std::string generateTestCode(size_t numTokens) {
  std::string code;
  code.reserve(numTokens * 10);
  
  for (size_t i = 0; i < numTokens; ++i) {
    // Alternate between different token types
    switch (i % 10) {
    case 0:
      code += "int ";
      break;
    case 1:
      code += generateIdentifier(8) + " ";
      break;
    case 2:
      code += "= ";
      break;
    case 3:
      code += std::to_string(i) + " ";
      break;
    case 4:
      code += "; ";
      break;
    case 5:
      code += "return ";
      break;
    case 6:
      code += generateIdentifier(6) + " ";
      break;
    case 7:
      code += "+ ";
      break;
    case 8:
      code += std::to_string(i * 2) + " ";
      break;
    case 9:
      code += ";\n";
      break;
    }
  }
  
  return code;
}

// Benchmark lexer performance
void benchmarkLexer(const std::string& code, const std::string& name) {
  SourceManager SM;
  std::string outputStr;
  llvm::raw_string_ostream outputStream(outputStr);
  DiagnosticsEngine Diags(outputStream);
  
  // Warmup
  Lexer Lex(SM, Diags, code, SM.createMainFileID("test.cpp", code));
  Token Tok;
  while (Lex.lexToken(Tok)) {}
  
  // Benchmark
  const int iterations = 10;
  auto start = std::chrono::high_resolution_clock::now();
  
  for (int i = 0; i < iterations; ++i) {
    Lexer Lex(SM, Diags, code, SM.createMainFileID("test.cpp", code));
    Token Tok;
    while (Lex.lexToken(Tok)) {}
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  double tokensPerSecond = (code.size() * iterations * 1000000.0) / duration.count();
  double mbPerSecond = tokensPerSecond / (1024 * 1024);
  
  llvm::outs() << name << ":\n";
  llvm::outs() << "  Code size: " << code.size() << " bytes\n";
  llvm::outs() << "  Total time: " << duration.count() << " μs\n";
  llvm::outs() << "  Throughput: " << mbPerSecond << " MB/s\n";
  llvm::outs() << "\n";
}

int main() {
  llvm::outs() << "=== BlockType Lexer Performance Benchmark ===\n\n";
  
  // Small file benchmark
  benchmarkLexer(generateTestCode(100), "Small file (100 tokens)");
  
  // Medium file benchmark
  benchmarkLexer(generateTestCode(1000), "Medium file (1000 tokens)");
  
  // Large file benchmark
  benchmarkLexer(generateTestCode(10000), "Large file (10000 tokens)");
  
  // Very large file benchmark
  benchmarkLexer(generateTestCode(100000), "Very large file (100000 tokens)");
  
  // Chinese keywords benchmark
  std::string chineseCode;
  for (int i = 0; i < 1000; ++i) {
    chineseCode += "整数 变量" + std::to_string(i) + " = " + std::to_string(i) + ";\n";
  }
  benchmarkLexer(chineseCode, "Chinese keywords (1000 tokens)");
  
  return 0;
}
