#pragma once

#include "blocktype/Basic/LLVM.h"
#include "blocktype/Basic/SourceLocation.h"
#include <gtest/gtest.h>
#include <string>
#include <cstdio>

namespace blocktype {
namespace test {

/// 创建临时文件
///
/// \param Content 文件内容
/// \param Ext 文件扩展名（默认 .cpp）
/// \return 临时文件路径
inline std::string createTempFile(StringRef Content, StringRef Ext = ".cpp") {
  // 创建临时文件名
  char TempPath[] = "/tmp/blocktype-test-XXXXXX";
  int FD = mkstemp(TempPath);
  if (FD == -1) {
    throw std::runtime_error("Failed to create temporary file");
  }
  
  // 写入内容
  ssize_t BytesWritten = write(FD, Content.data(), Content.size());
  close(FD);
  
  if (static_cast<size_t>(BytesWritten) != Content.size()) {
    throw std::runtime_error("Failed to write to temporary file");
  }
  
  return std::string(TempPath);
}

/// 从字符串解析的辅助类
class ParseHelper {
  std::unique_ptr<llvm::MemoryBuffer> Buffer;
  
public:
  /// 构造函数
  explicit ParseHelper(StringRef Code) {
    Buffer = llvm::MemoryBuffer::getMemBufferCopy(Code);
  }
  
  /// 获取缓冲区
  llvm::MemoryBuffer* getBuffer() const { return Buffer.get(); }
  
  /// 获取缓冲区内容
  StringRef getCode() const { return Buffer->getBuffer(); }
};

/// 测试夹具基类
class BlockTypeTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 初始化测试环境
  }
  
  void TearDown() override {
    // 清理测试环境
  }
};

/// 断言两个字符串相等（忽略空白差异）
inline void expectStrEq(StringRef Expected, StringRef Actual) {
  // 移除首尾空白
  Expected = Expected.trim();
  Actual = Actual.trim();
  
  EXPECT_EQ(Expected.str(), Actual.str());
}

/// 断言字符串包含子串
inline void expectStrContains(StringRef Haystack, StringRef Needle) {
  EXPECT_TRUE(Haystack.contains(Needle))
    << "Expected '" << Haystack.str() << "' to contain '" << Needle.str() << "'";
}

} // namespace test
} // namespace blocktype
