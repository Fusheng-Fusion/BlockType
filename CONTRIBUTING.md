# 贡献指南

感谢您对 BlockType 项目的关注！本文档将帮助您了解如何为项目做出贡献。

## 行为准则

- 尊重所有贡献者
- 保持专业和友好的交流
- 接受建设性批评
- 关注对社区最有利的事情

## 如何贡献

### 报告问题

如果您发现了 bug 或有功能建议，请：

1. 检查 [Issues](https://github.com/blocktype/blocktype/issues) 是否已有相同问题
2. 如果没有，创建新的 Issue，包含：
   - 清晰的标题
   - 详细的问题描述
   - 重现步骤（如果是 bug）
   - 期望行为和实际行为
   - 环境信息（操作系统、编译器版本等）

### 提交代码

1. **Fork 仓库**
   ```bash
   git clone https://github.com/YOUR_USERNAME/blocktype.git
   cd blocktype
   ```

2. **创建分支**
   ```bash
   git checkout -b feature/your-feature-name
   # 或
   git checkout -b fix/your-bug-fix
   ```

3. **编写代码**
   - 遵循代码风格指南
   - 添加必要的测试
   - 更新相关文档

4. **运行测试**
   ```bash
   ./scripts/build.sh Debug
   ./scripts/test.sh build-debug
   ```

5. **提交更改**
   ```bash
   git add .
   git commit -m "feat: 添加新功能描述"
   # 或
   git commit -m "fix: 修复问题描述"
   ```

6. **推送分支**
   ```bash
   git push origin feature/your-feature-name
   ```

7. **创建 Pull Request**
   - 在 GitHub 上创建 PR
   - 填写 PR 模板
   - 等待代码审查

## 代码风格

### C++ 代码风格

项目使用 LLVM 代码风格，主要规则：

```cpp
// 类名：PascalCase
class MyClass {};

// 函数名：camelCase
void doSomething();

// 变量名：camelCase
int myVariable;

// 常量：UPPER_CASE
const int MAX_SIZE = 100;

// 成员变量：尾部下划线
class MyClass {
  int member_;
};

// 命名空间：小写
namespace blocktype {}

// 枚举值：PascalCase
enum class Color {
  Red,
  Green,
  Blue
};
```

### 格式化

使用项目根目录的 `.clang-format` 配置：

```bash
# 格式化单个文件
clang-format -i path/to/file.cpp

# 格式化所有文件
find . -name '*.cpp' -o -name '*.h' | xargs clang-format -i
```

### 代码检查

使用 `.clang-tidy` 配置进行检查：

```bash
# 需要先构建项目生成 compile_commands.json
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy path/to/file.cpp -- -p build
```

## 提交信息规范

使用 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 类型 (type)

- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式调整（不影响功能）
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关
- `perf`: 性能优化

### 示例

```
feat(lexer): 添加中文关键字支持

- 支持 "如果"、"否则"、"循环" 等中文关键字
- 更新关键字映射表
- 添加相关测试

Closes #123
```

```
fix(sema): 修复模板实例化时的类型推导错误

在特定情况下，模板实例化时会错误地推导参数类型。
现在通过改进类型匹配逻辑解决了这个问题。

Fixes #456
```

## 测试要求

### 单元测试

- 新功能必须包含单元测试
- 测试覆盖率目标：80%+
- 使用 Google Test 框架

```cpp
// tests/unit/Basic/SourceLocationTest.cpp
#include <gtest/gtest.h>
#include "blocktype/Basic/SourceLocation.h"

TEST(SourceLocationTest, DefaultInvalid) {
  SourceLocation Loc;
  EXPECT_FALSE(Loc.isValid());
  EXPECT_TRUE(Loc.isInvalid());
}
```

### 回归测试

- 重要功能应添加回归测试
- 使用 LLVM Lit 框架

```lit
// tests/lit/basic/version.test
// RUN: %blocktype --version | FileCheck %s

// CHECK: blocktype version {{[0-9]+\.[0-9]+\.[0-9]+}}
```

## 文档要求

### 代码注释

```cpp
/// 判断字符是否可以作为标识符起始字符
///
/// 根据 UAX #31 标准判断，支持：
/// - ASCII 字母和下划线
/// - 中文字符
/// - Unicode ID_Start 字符
///
/// \param CP Unicode 码点
/// \return 如果可以作为起始字符返回 true
bool isIDStart(uint32_t CP);
```

### README 更新

如果您的更改影响：
- 构建步骤
- 依赖项
- 使用方式
- 配置选项

请更新 README.md。

## 开发环境设置

### 系统要求

- **操作系统**: Linux (Ubuntu 22.04+) / macOS (14+) / Windows (10+)
- **编译器**: Clang 18+ / GCC 13+ / MSVC 19.35+
- **CMake**: 3.20+
- **LLVM**: 18+

### 依赖安装

#### Linux (Ubuntu)

```bash
# LLVM
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18

# ICU
sudo apt-get install libicu-dev

# libcurl
sudo apt-get install libcurl4-openssl-dev
```

#### macOS

```bash
# LLVM
brew install llvm@18

# ICU
brew install icu4c

# libcurl (系统自带)
```

### 构建项目

```bash
# 克隆仓库
git clone https://github.com/blocktype/blocktype.git
cd blocktype

# 构建
./scripts/build.sh Debug

# 运行测试
./scripts/test.sh build-debug
```

## 发布流程

1. 更新版本号（`CMakeLists.txt`）
2. 更新 CHANGELOG.md
3. 创建 Git tag
4. 构建 release 版本
5. 创建 GitHub Release

## 获取帮助

- **文档**: 查看 `docs/` 目录
- **Issues**: 在 GitHub Issues 提问
- **讨论**: 在 GitHub Discussions 讨论

## 许可证

通过贡献代码，您同意您的代码将采用与项目相同的许可证（MIT License）。

---

再次感谢您的贡献！🎉
