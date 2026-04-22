# BlockType 测试基线报告

**生成时间**: 2026-04-23  
**生成人**: tester（测试人员）  
**项目版本**: 0.1.0  
**项目阶段**: Phase 0-6 并行开发中

---

## 1. 测试基础设施概览

### 1.1 测试框架

| 框架 | 用途 | 状态 |
|------|------|------|
| Google Test (gtest) | 单元测试 + 集成测试 | ✅ 已集成 |
| CTest | 测试运行器 | ✅ 已集成 |
| Lit + FileCheck | 端到端测试 | ⚠️ 部分配置 |
| 自定义基准测试 | 性能回归检测 | ✅ 已集成 |
| lcov/genhtml | 代码覆盖率 | ✅ 已集成 |

### 1.2 构建与测试命令

```bash
# 构建
./scripts/build.sh [Release|Debug]

# 运行测试
./scripts/test.sh [build-release|build-debug]

# 性能回归检测
python3 scripts/check_performance.py [--verbose]

# Phase 验证
python3 scripts/phase_validator.py --phase <N> --task <T>

# 覆盖率报告（需 ENABLE_COVERAGE=ON）
cmake -B build -DENABLE_COVERAGE=ON && cmake --build build
cd build && ctest && make coverage
```

---

## 2. 测试用例清单

### 2.1 单元测试 (tests/unit/)

| 模块 | 文件数 | 测试文件 | CMake 集成 |
|------|--------|----------|------------|
| **Basic** | 4 | SourceLocationTest, DiagnosticsTest, FixItHintTest, UTF8ValidatorTest | ✅ |
| **Lex** | 10 | LexerTest, TokenTest, PreprocessorTest, LexerFixTest, LexerExtensionTest, BoundaryCaseTest, HighPriorityFixesTest, MediumPriorityFixesTest | ✅ |
| **AST** | 5 | ASTTest, Stage71Test, Stage72Test, Stage73Test | ✅ |
| **Parse** | 6 | ParserTest, DeclarationTest, StatementTest, AccessControlTest, ErrorRecoveryTest | ✅ |
| **Sema** | 13 | SemaTest, TypeCheckTest, NameLookupTest, OverloadResolutionTest, TemplateDeductionTest, TemplateInstantiationTest, VariadicTemplateTest, ConceptTest, SFINAETest, ConstantExprTest, SymbolTableTest, AccessControlTest | ✅ |
| **CodeGen** | 9 | CodeGenFunctionTest, CodeGenExprTest, CodeGenStmtTest, CodeGenTypesTest, CodeGenClassTest, CodeGenConstantTest, CodeGenDebugInfoTest, ManglerTest | ✅ |
| **Module** | 3 | ModuleManagerTest, ModuleLinkerTest, ModuleVisibilityTest | ✅ |
| **Frontend** | 1 | InfrastructureSharingTest | ✅ |
| **AI** | 3 | ResponseCacheTest, CostTrackerTest, AIOrchestratorTest | ✅ |
| **Performance** | 1 | PerformanceTest | ✅ |

**单元测试总计**: 约 54 个测试文件

### 2.2 集成测试 (tests/integration/)

| 模块 | 文件数 | 测试文件 | CMake 集成 |
|------|--------|----------|------------|
| **AI** | 1 | ProviderIntegrationTest | ✅ |

**集成测试总计**: 1 个测试文件

### 2.3 Lit 端到端测试 (tests/lit/)

| 模块 | 文件数 | 测试文件 |
|------|--------|----------|
| **lex** | 4 | chinese-keywords, keywords, literals, operators |
| **parser** | 9 | basic, class, concept, declarations, enum, errors, namespace, template, using |
| **CodeGen** | 12 | arithmetic, basic-types, class-layout, control-flow, cpp23-features, cpp26-contracts, cpp26-reflection, debug-info, function-call, inheritance, virtual-call, virtual-inheritance |

**Lit 测试总计**: 25 个 .test 文件

### 2.4 基准测试 (tests/benchmark/)

| 测试 | 文件 | CMake 集成 |
|------|------|------------|
| Lexer 基准 | LexerBenchmark.cpp | ✅ |
| Parser 基准 | ParserBenchmark.cpp | ✅ |

### 2.5 未纳入构建的测试

| 目录 | 文件数 | 说明 |
|------|--------|------|
| tests/cpp26/ | 34 | C++26 特性测试，未纳入 CMake |
| tests/ 根目录散落文件 | ~30 | 调试/临时测试文件，未纳入 CMake |

---

## 3. 已知测试失败

### 3.1 test_result.xml (2026-04-16)

| 测试用例 | 状态 | 详情 |
|----------|------|------|
| ParserTest.TemplateSpecializationWithBuiltinType | ❌ 失败 | `llvm::isa<TemplateSpecializationExpr>(E)` 返回 false，期望 true |

### 3.2 test_results.xml (2026-04-21)

| 测试用例 | 状态 | 详情 |
|----------|------|------|
| LexerFixTest.HexLiteralWithDigitSeparator | ❌ 失败 | `Diags.hasErrorOccurred()` 返回 true，期望 false（十六进制数字分隔符词法分析错误） |

### 3.3 失败分析

1. **ParserTest.TemplateSpecializationWithBuiltinType**: 解析器未能正确识别模板特化表达式，可能是因为模板特化解析逻辑不完整或 AST 节点类型不匹配
2. **LexerFixTest.HexLiteralWithDigitSeparator**: 词法分析器不支持十六进制字面量中的数字分隔符（如 `0xFF'FF'FF`），这是一个 C++14 特性

---

## 4. 测试覆盖分析

### 4.1 模块覆盖矩阵

| 编译器阶段 | 单元测试 | 集成测试 | Lit 测试 | 覆盖评估 |
|------------|----------|----------|----------|----------|
| Basic (基础设施) | ✅ 4个 | - | - | 🟡 基本 |
| Lex (词法分析) | ✅ 10个 | - | ✅ 4个 | 🟢 较好 |
| Parse (语法分析) | ✅ 6个 | - | ✅ 9个 | 🟢 较好 |
| AST (抽象语法树) | ✅ 5个 | - | - | 🟡 基本 |
| Sema (语义分析) | ✅ 13个 | - | - | 🟢 较好 |
| CodeGen (代码生成) | ✅ 9个 | - | ✅ 12个 | 🟢 较好 |
| Module (模块系统) | ✅ 3个 | - | - | 🟡 基本 |
| Frontend (前端) | ✅ 1个 | - | - | 🔴 不足 |
| AI (AI 集成) | ✅ 3个 | ✅ 1个 | - | 🟡 基本 |
| Driver (驱动器) | - | - | - | 🔴 缺失 |

### 4.2 覆盖缺口

1. **Frontend 模块**: 仅 1 个测试文件（InfrastructureSharingTest），缺少编译流程端到端测试
2. **Driver 模块**: 无专门测试，编译主流程未被测试覆盖
3. **双语支持**: 仅 lit/lex/chinese-keywords.test 覆盖中文关键字，缺少中文标识符、中文诊断消息等测试
4. **错误恢复**: Parse/ErrorRecoveryTest 存在但覆盖面未知
5. **C++26 特性**: 34 个测试文件未纳入构建，无法验证
6. **端到端流程**: 缺少从源代码到 LLVM IR 完整编译流程的集成测试

---

## 5. 测试基础设施问题

| 问题 | 严重程度 | 说明 |
|------|----------|------|
| 根目录散落调试文件 | 中 | ~30 个未纳入构建的测试文件，可能是临时调试产物 |
| cpp26 测试未集成 | 中 | 34 个 C++26 特性测试未纳入 CMake 构建 |
| Lit 测试框架未完全配置 | 中 | ROADMAP 标记为未完成，需 lit 工具安装 |
| 无 CI/CD | 高 | 无自动化持续集成，测试需手动运行 |
| 测试结果文件过时 | 低 | test_result.xml 和 test_results.xml 非最新 |
| 集成测试不足 | 高 | 仅 1 个集成测试文件 |

---

## 6. 测试辅助工具

| 工具 | 路径 | 用途 |
|------|------|------|
| TestHelpers.h | tests/TestHelpers.h | 临时文件创建、ParseHelper、BlockTypeTest 基类、字符串断言 |
| phase_validator.py | scripts/phase_validator.py | Phase 完成验证（支持 Task 2.1, 2.3） |
| check_performance.py | scripts/check_performance.py | 性能回归检测（Lexer 吞吐量、缓存加速比） |
| verify_parser_completeness.py | scripts/verify_parser_completeness.py | Parser 函数完整性验证 |

---

## 7. 测试标准与规范

### 7.1 测试用例编写规范

1. **单元测试**: 使用 gtest 框架，每个模块一个目录，文件名以 `*Test.cpp` 结尾
2. **Lit 测试**: 使用 `// RUN:` 和 `// CHECK:` 指令，文件名以 `.test` 结尾
3. **测试夹具**: 继承 `BlockTypeTest` 基类或自定义 `::testing::Test` 子类
4. **辅助工具**: 使用 `TestHelpers.h` 中的 `createTempFile`、`ParseHelper` 等

### 7.2 Bug 报告模板

```markdown
## Bug 报告

**严重程度**: [P0-阻塞/P1-严重/P2-中等/P3-轻微]

### 复现步骤
1. ...

### 预期结果
...

### 实际结果
...

### 环境信息
- 操作系统: 
- 构建类型: 
- LLVM 版本: 

### 相关代码
- 文件: 
- 行号: 
```

---

## 8. 下一步行动

1. ⏳ 等待规划人员完成规划
2. 🔲 根据规划制定详细测试计划
3. 🔲 运行完整测试套件获取最新基线
4. 🔲 修复已知失败测试用例（需开发人员配合）
5. 🔲 补充覆盖缺口测试
6. 🔲 将 cpp26 测试纳入 CMake 构建
