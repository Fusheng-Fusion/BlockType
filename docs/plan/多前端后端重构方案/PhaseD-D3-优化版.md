# Phase D.3 — 保留旧路径作为 fallback（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **Fallback 检测逻辑矛盾**：
   - Spec 说"当 `--frontend` 和 `--backend` 都未指定时，使用旧路径"。
   - 但 D.1 中 `FrontendName` 和 `BackendName` 有默认值 `"cpp"` 和 `"llvm"`，导致字段始终有值。
   - **修复**：使用 D.1 新增的 `isFrontendExplicitlySet()` / `isBackendExplicitlySet()` 判断用户是否显式指定。只有当两者都未显式指定时，走旧路径。

2. **Spec 的 fallback 条件"当指定了任一选项时使用新路径"过于激进**：
   - 如果用户只指定 `--backend=llvm`（值等于默认值），旧路径的 `CppFrontend` → `LLVMBackend` 新路径尚未稳定，直接切换风险大。
   - **修复**：改为"当两者都显式指定时走新路径；只指定一个时，用新管线但补充默认值；都未指定走旧路径"。

### P2 — 建议修复
1. **旧路径代码的标记不明确**：Spec 说标记 `// DEPRECATED: old pipeline`，但应明确标记的范围是整个 `runOldPipeline()` 方法还是每一行。
2. **新旧路径结果一致性验证**：Spec 的验收标准要求 `objdump -d` 输出完全一致，但编译器输出的二进制可能有微小差异（如时间戳、UUID）。

### P3 — 建议改进
1. 添加编译选项 `--use-new-pipeline` 强制使用新管线（无需 --frontend/--backend），方便测试。

## 依赖分析

- **前置依赖**：D.2（CompilerInstance 重构）
- **后续依赖**：D.4（自动注册）、D.5（集成测试）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `src/Frontend/CompilerInstance.cpp` |

## 当前代码状态

- `CompilerInstance::compileFile()` 当前只有旧管线（单一流程）。
- D.2 将新增 `runNewPipeline()` 方法，旧管线代码仍在 `compileFile()` 中。
- 需要提取旧管线代码到 `runOldPipeline()` 方法，然后在 `compileFile()` 中做分流。

## 实现步骤

1. **提取旧管线**：将当前 `compileFile()` 的全部逻辑提取到 `runOldPipeline(StringRef Filename)` 方法中。

2. **修改 `compileFile()` 入口**：
   ```cpp
   bool CompilerInstance::compileFile(StringRef Filename) {
     if (Invocation->isFrontendExplicitlySet() || Invocation->isBackendExplicitlySet()) {
       return runNewPipeline(Filename);
     }
     // DEPRECATED: old pipeline (保留到 Phase E.3)
     return runOldPipeline(Filename);
   }
   ```

3. **标记旧管线代码**：在 `runOldPipeline()` 方法顶部添加注释：
   ```cpp
   // DEPRECATED: old pipeline — 将在 Phase E.3 移除
   // 此方法保留 AST→LLVM IR→目标代码 的旧编译路径
   ```

4. **确保新管线通过 `runNewPipeline()` 调用**（D.2 已实现）。

5. **验证新旧路径各自独立工作**。

## 接口签名（最终版）

```cpp
class CompilerInstance {
  // ...
  bool compileFile(StringRef Filename);

private:
  bool runNewPipeline(StringRef Filename);   // Phase D.2 新增
  bool runOldPipeline(StringRef Filename);   // Phase D.3 从 compileFile() 提取
  // ...
};
```

## 测试计划

```cpp
// T1: 旧路径可用
// CompilerInvocation 不设 FrontendExplicitlySet / BackendExplicitlySet
// compileFile("test.cpp") 走 runOldPipeline()
// 退出码 == 0

// T2: 新路径可用
// CompilerInvocation 设置 setFrontendName("cpp"), setBackendName("llvm")
// compileFile("test.cpp") 走 runNewPipeline()
// 退出码 == 0

// T3: 只指定 --backend=llvm 走新路径
// 设置 setBackendName("llvm")，不设 setFrontendName
// compileFile("test.cpp") 走 runNewPipeline()（因为 BackendExplicitlySet=true）

// T4: 新旧路径均可编译同一文件
// 两条路径分别编译 test.cpp，均生成有效目标文件
```

## 风险点

1. **旧管线代码提取**：`compileFile()` 中有一些共享逻辑（如 `readFileContent`、`Initialized` 检查），提取时需确保不遗漏。
2. **多文件编译**：`compileAllFiles()` 也需要同步修改，分流新旧管线。
3. **链接步骤**：新管线的链接是否通过 `Backend->emitObject()` 后调用系统链接器？需确认。
