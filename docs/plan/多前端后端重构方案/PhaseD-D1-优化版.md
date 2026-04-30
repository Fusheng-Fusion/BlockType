# Phase D.1 — CompilerInvocation 新增 FrontendName/BackendName（优化版）

## 原始 Spec 问题

### P1 — 必须修复
- **无 P1 问题**。该任务相对简单，仅新增字段。

### P2 — 建议修复
1. **Spec 未提及命令行解析的集成方式**：当前 `CompilerInvocation::parseCommandLine()` 和 `parseFromCommandLine()` 已有命令行解析逻辑，需明确 `--frontend` 和 `--backend` 在哪个方法中解析。
2. **与 D.3 fallback 检测冲突**：默认值 `"cpp"` / `"llvm"` 会导致 D.3 无法通过字段值判断"用户是否显式指定"。建议新增 `bool FrontendExplicitlySet = false` / `bool BackendExplicitlySet = false` 标记。

### P3 — 建议改进
1. 字段应放在 `FrontendOptions` 或新增 `PipelineOptions` struct 中，而非直接放在 `CompilerInvocation` 类上——保持现有的 struct 分组风格（LangOpts, TargetOpts, CodeGenOpts, DiagOpts, AIOpts, FrontendOpts）。

## 依赖分析

- **前置依赖**：Phase C.1（BackendBase/BackendRegistry），Phase B.1/B.2（FrontendBase/FrontendRegistry）
- **后续依赖**：D.2（CompilerInstance 重构）、D.3（fallback 检测）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `include/blocktype/Frontend/CompilerInvocation.h` |
| 修改 | `src/Frontend/CompilerInvocation.cpp` |

## 当前代码状态

- `CompilerInvocation` 是纯数据类，持有 `LangOpts`, `TargetOpts`, `CodeGenOpts`, `DiagOpts`, `AIOpts`, `FrontendOpts` 六个 struct。
- **无** `FrontendName` / `BackendName` 字段。
- `parseCommandLine()` 和 `parseFromCommandLine()` 方法负责解析命令行。
- 当前命令行选项中**无** `--frontend` / `--backend`。

## 实现步骤

1. 在 `CompilerInvocation` 类中新增字段：
   ```cpp
   // === Pipeline options (Phase D) ===
   std::string FrontendName = "cpp";
   std::string BackendName = "llvm";
   bool FrontendExplicitlySet = false;
   bool BackendExplicitlySet = false;
   ```
2. 新增 getter/setter 方法：
   ```cpp
   StringRef getFrontendName() const { return FrontendName; }
   StringRef getBackendName() const { return BackendName; }
   void setFrontendName(StringRef N) { FrontendName = N.str(); FrontendExplicitlySet = true; }
   void setBackendName(StringRef N) { BackendName = N.str(); BackendExplicitlySet = true; }
   bool isFrontendExplicitlySet() const { return FrontendExplicitlySet; }
   bool isBackendExplicitlySet() const { return BackendExplicitlySet; }
   ```
3. 在 `parseCommandLine()` 中新增 `--frontend=<name>` 和 `--backend=<name>` 选项解析。当用户传了这些选项时调用 setter（自动设置 ExplicitlySet 标记）。
4. 在 `validate()` 中新增验证：检查 FrontendName/BackendName 对应的注册表中是否存在该名称。
5. 在 `toString()` / `fromString()` 中序列化新字段。

## 接口签名（最终版）

```cpp
// 在 class CompilerInvocation 内新增：

  // === Pipeline options (Phase D) ===
  /// Frontend name (default "cpp").
  std::string FrontendName = "cpp";
  /// Backend name (default "llvm").
  std::string BackendName = "llvm";
  /// Whether --frontend was explicitly specified.
  bool FrontendExplicitlySet = false;
  /// Whether --backend was explicitly specified.
  bool BackendExplicitlySet = false;

  StringRef getFrontendName() const;
  StringRef getBackendName() const;
  void setFrontendName(StringRef N);
  void setBackendName(StringRef N);
  bool isFrontendExplicitlySet() const;
  bool isBackendExplicitlySet() const;
```

## 测试计划

```cpp
// T1: 默认值
CompilerInvocation CI;
assert(CI.getFrontendName() == "cpp");
assert(CI.getBackendName() == "llvm");
assert(CI.isFrontendExplicitlySet() == false);
assert(CI.isBackendExplicitlySet() == false);

// T2: 显式设置
CI.setFrontendName("bt");
CI.setBackendName("cranelift");
assert(CI.getFrontendName() == "bt");
assert(CI.getBackendName() == "cranelift");
assert(CI.isFrontendExplicitlySet() == true);
assert(CI.isBackendExplicitlySet() == true);

// T3: 命令行解析
const char* Args[] = {"blocktype", "--frontend=cpp", "--backend=llvm", "test.cpp"};
CI.parseCommandLine(4, Args);
assert(CI.isFrontendExplicitlySet() == true);
assert(CI.isBackendExplicitlySet() == true);
```

## 风险点

1. **命令行解析库兼容性**：当前使用 LLVM CommandLine 库（`cl::ParseCommandLineOptions`），需确保 `--frontend` 和 `--backend` 选项名称不与现有选项冲突。
2. **validate() 的注册表依赖**：如果 validate() 检查注册表，则编译时必须有对应的静态注册已执行。Phase D.4 负责注册，但 D.1 可能先于注册执行——建议 validate() 仅做非空检查，不做注册表查询。
