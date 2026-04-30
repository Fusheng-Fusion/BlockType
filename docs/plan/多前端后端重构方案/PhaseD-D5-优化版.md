# Phase D.5 — 集成测试（优化版）

## 原始 Spec 问题

### P1 — 必须修复
- **无 P1 问题**。测试任务相对简单。

### P2 — 建议修复
1. **Spec 的验收标准 V1/V2 使用 shell 命令**，但测试框架是 GTest/lit。应提供具体的 GTest 测试用例。
2. **差分测试 V2 对比旧路径和新路径**，但旧路径在 D.3 之后需要 `--no-frontend/--no-backend` 或某种方式显式触发。Spec 未说明如何触发旧路径。
3. **Spec 未列出测试文件产出路径**。

### P3 — 建议改进
1. 增加 `CompilerInvocation` 命令行解析的端到端测试。
2. 增加错误路径测试（指定不存在的前端/后端名称）。

## 依赖分析

- **前置依赖**：D.1 ~ D.4 全部完成

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `tests/integration/pipeline/D2PipelineTest.cpp` |
| 新增 | `tests/integration/pipeline/D3FallbackTest.cpp` |
| 新增 | `tests/integration/pipeline/D4RegistrationTest.cpp` |

## 当前代码状态

- `tests/integration/` 目录结构已存在（Phase C 的后端测试在此目录下）。
- 现有测试框架：GTest（单元测试）+ lit（端到端测试）。

## 实现步骤

1. **创建 `D2PipelineTest.cpp`**（新管线端到端测试）：
   - 测试 `--frontend=cpp --backend=llvm` 完整编译流程。
   - 测试 `CompilerInvocation` 新字段的默认值和显式设置。
   - 测试 `FrontendRegistry::create()` + `BackendRegistry::create()` 联合使用。
   - 测试 `CompilerInstance::compileFile()` 走新管线。

2. **创建 `D3FallbackTest.cpp`**（旧路径 fallback 测试）：
   - 测试不指定 `--frontend/--backend` 时走旧路径。
   - 测试指定任一选项时走新路径。
   - 测试新旧路径对同一源文件均可生成有效目标文件。

3. **创建 `D4RegistrationTest.cpp`**（注册测试）：
   - 测试 `FrontendRegistry::instance().hasFrontend("cpp")`。
   - 测试 `BackendRegistry::instance().hasBackend("llvm")`。
   - 测试 `BackendRegistry::instance().create("llvm", Opts, Diags)` 返回有效实例。

4. **在 `CMakeLists.txt` 中注册新测试**。

## 测试计划

```cpp
// === D2PipelineTest.cpp ===

TEST(D2Pipeline, DefaultInvocationValues) {
  CompilerInvocation CI;
  EXPECT_EQ(CI.getFrontendName(), "cpp");
  EXPECT_EQ(CI.getBackendName(), "llvm");
  EXPECT_FALSE(CI.isFrontendExplicitlySet());
  EXPECT_FALSE(CI.isBackendExplicitlySet());
}

TEST(D2Pipeline, ExplicitSetInvocation) {
  CompilerInvocation CI;
  CI.setFrontendName("bt");
  CI.setBackendName("cranelift");
  EXPECT_EQ(CI.getFrontendName(), "bt");
  EXPECT_TRUE(CI.isFrontendExplicitlySet());
  EXPECT_TRUE(CI.isBackendExplicitlySet());
}

TEST(D2Pipeline, NewPipelineCompile) {
  // 构造 CompilerInvocation
  // 设置 setFrontendName("cpp"), setBackendName("llvm")
  // 创建 CompilerInstance, 初始化, compileFile("test.cpp")
  // 验证目标文件生成
}

// === D3FallbackTest.cpp ===

TEST(D3Fallback, OldPipelineWorks) {
  // 不设 FrontendExplicitlySet / BackendExplicitlySet
  // compileFile("test.cpp") 应走旧管线
}

TEST(D3Fallback, NewPipelineWhenExplicit) {
  // 设置 setFrontendName("cpp")
  // compileFile("test.cpp") 应走新管线
}

// === D4RegistrationTest.cpp ===

TEST(D4Registration, FrontendRegistered) {
  EXPECT_TRUE(frontend::FrontendRegistry::instance().hasFrontend("cpp"));
}

TEST(D4Registration, BackendRegistered) {
  EXPECT_TRUE(backend::BackendRegistry::instance().hasBackend("llvm"));
}

TEST(D4Registration, CreateBackend) {
  DiagnosticsEngine Diags;
  backend::BackendOptions Opts;
  auto BE = backend::BackendRegistry::instance().create("llvm", Opts, Diags);
  ASSERT_NE(BE, nullptr);
  EXPECT_EQ(BE->getName(), "llvm");
}
```

## 风险点

1. **测试依赖真实编译环境**：集成测试可能需要 LLVM 工具链（如 `clang++` 链接器）。确保 CI 环境中有对应工具。
2. **文件路径依赖**：测试用例中引用的源文件需在测试目录中准备好。
3. **并发安全**：多个测试并发运行时，全局单例注册不应冲突（Phase D.4 使用静态初始化，一次注册）。
