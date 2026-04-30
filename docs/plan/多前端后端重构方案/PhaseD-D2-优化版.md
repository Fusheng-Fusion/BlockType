# Phase D.2 — CompilerInstance 重构为管线编排器（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **Spec 的 `CompilerInstance` 新成员列表与实际代码差距巨大**：
   - 当前 `CompilerInstance` 持有 `SourceMgr`, `Context`, `PP`, `SemaPtr`, `ParserPtr`, `LLVMCtx` — 这些是旧管线组件。
   - Spec 仅列出 `Frontend`, `Backend`, `IRCtx`, `IRModule`, `IRTypeCtx`, `Layout` — 缺少旧组件的保留说明（D.3 需要旧路径）。
   - **修复**：应保留旧组件，新增新组件，在 `compileFile()` 中根据 `isFrontendExplicitlySet()/isBackendExplicitlySet()` 分流。

2. **`runFrontend(Filename)` 签名不完整**：
   - 实际 `FrontendBase::compile()` 签名为 `compile(StringRef Filename, IRTypeContext& TypeCtx, const TargetLayout& Layout)`。
   - Spec 的 `runFrontend(StringRef Filename)` 隐去了 `IRTypeContext` 和 `TargetLayout` 参数。
   - **修复**：`runFrontend()` 需先创建 `IRTypeContext` 和 `TargetLayout`，再调用 `Frontend->compile()`。

3. **Spec 未说明 `IRTypeContext`/`TargetLayout` 的创建方式**：
   - `IRTypeContext` 需要在调用 `FrontendBase::compile()` 之前创建。
   - `TargetLayout` 需要基于 `TargetTriple` 构建。
   - **修复**：在 `compileFile()` 流程中明确创建步骤。

4. **Spec 说 `FrontendBase::compile()` 返回 `IRModule*`**：
   - 实际签名返回 `std::unique_ptr<ir::IRModule>`（值语义转移所有权）。
   - **修复**：接口签名应与实际实现一致。

5. **Spec 列出 `CompilerInstance` 持有 `unique_ptr<ir::IRContext>` 和 `unique_ptr<ir::IRModule>`**：
   - `IRContext` 和 `IRModule` 的关系需明确：`IRContext` 可拥有多个 `IRModule`。
   - **修复**：改为 `IRContext` 拥有 `IRModule`，`CompilerInstance` 仅持有 `IRContext`。

### P2 — 建议修复
1. **Spec 列出 `shared_ptr<CompilerInvocation>` 但未说明原因**：当前已用 `shared_ptr`，保持一致即可。
2. **Spec 未提及 `Telemetry` 的集成**：当前 `CompilerInstance` 持有 `TelemetryCollector`，新管线也应保持遥测能力。

### P3 — 建议改进
1. 旧组件（`SourceMgr`, `Context`, `PP`, `SemaPtr`, `ParserPtr`）在新管线下不需要，但 D.3 保留旧路径需要它们——建议用条件编译或 flag 控制。

## 依赖分析

- **前置依赖**：D.1（CompilerInvocation 新字段）
- **软依赖**：C.5（LLVMBackend，已实现）、B.10（CppFrontend，已实现）

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `include/blocktype/Frontend/CompilerInstance.h` |
| 修改 | `src/Frontend/CompilerInstance.cpp` |

## 当前代码状态

- `CompilerInstance.h` 直接 `#include "llvm/IR/LLVMContext.h"`。
- `CompilerInstance` 直接持有 `unique_ptr<llvm::LLVMContext> LLVMCtx`。
- `compileFile()` 旧流程：loadSourceFile → parseTranslationUnit → performSemaAnalysis → generateLLVMIR → runOptimizationPasses → generateObjectFile。
- `CompilerInstance.cpp` 直接调用 LLVM API（PassBuilder, TargetMachine 等）。
- `FrontendBase::compile()` 需要 `IRTypeContext&` 和 `const TargetLayout&`。
- `BackendRegistry::create()` 需要 `(Name, BackendOptions, Diags)`。
- `FrontendRegistry::create()` 需要 `(Name, FrontendCompileOptions, Diags)`。

## 实现步骤

1. **在 `CompilerInstance.h` 中新增成员**（保留旧成员供 D.3 fallback）：
   ```cpp
   // === New pipeline components (Phase D) ===
   std::unique_ptr<frontend::FrontendBase> Frontend;
   std::unique_ptr<backend::BackendBase> Backend;
   std::unique_ptr<ir::IRContext> IRCtx;
   std::unique_ptr<ir::IRTypeContext> IRTypeCtx;
   std::unique_ptr<ir::TargetLayout> Layout;
   ```

2. **修改 `CompilerInstance.h` 的 `#include` 列表**：
   - 新增 `#include "blocktype/Frontend/FrontendBase.h"`
   - 新增 `#include "blocktype/Frontend/FrontendRegistry.h"`
   - 新增 `#include "blocktype/Backend/BackendBase.h"`
   - 新增 `#include "blocktype/Backend/BackendRegistry.h"`
   - 新增 `#include "blocktype/IR/IRContext.h"`
   - 新增 `#include "blocktype/IR/IRTypeContext.h"`
   - 新增 `#include "blocktype/IR/TargetLayout.h"`

3. **新增私有方法**：
   ```cpp
   bool runNewPipeline(StringRef Filename);
   bool runFrontend(StringRef Filename);
   bool runIRPipeline();
   bool runBackend(StringRef OutputPath);
   ```

4. **实现 `runNewPipeline()`**：
   ```
   a. 根据 Invocation->getFrontendName() 构造 FrontendCompileOptions
   b. 调用 FrontendRegistry::create(Name, FEOpts, *Diags) 创建 Frontend
   c. 根据 Invocation->getTargetOpts().Triple 创建 TargetLayout
   d. 创建 IRTypeContext
   e. 调用 Frontend->compile(Filename, *IRTypeCtx, *Layout) → 获取 IRModule
   f. 将 IRModule 传入 BackendRegistry::create() 构造的 Backend
   g. 调用 Backend->emitObject(*IRModule, OutputPath)
   ```

5. **修改 `compileFile()` 入口**：
   ```cpp
   bool CompilerInstance::compileFile(StringRef Filename) {
     if (Invocation->isFrontendExplicitlySet() || Invocation->isBackendExplicitlySet()) {
       return runNewPipeline(Filename);
     }
     // 旧管线...
   }
   ```

6. **暂不修改旧管线代码**（D.3 负责 fallback 整合）。

7. **目标**：D.2 完成后，`CompilerInstance.h` 仍可包含 `llvm/IR/LLVMContext.h`（因为旧管线仍在），但新管线方法不直接使用 LLVM API。

## 接口签名（最终版）

```cpp
class CompilerInstance {
  // === 旧管线组件（保留给 D.3 fallback） ===
  std::shared_ptr<CompilerInvocation> Invocation;
  std::unique_ptr<SourceManager> SourceMgr;
  std::unique_ptr<DiagnosticsEngine> Diags;
  std::unique_ptr<ASTContext> Context;
  std::unique_ptr<Preprocessor> PP;
  std::unique_ptr<Sema> SemaPtr;
  std::unique_ptr<Parser> ParserPtr;
  std::unique_ptr<llvm::LLVMContext> LLVMCtx;
  std::unique_ptr<telemetry::TelemetryCollector> Telemetry;
  TranslationUnitDecl *CurrentTU = nullptr;
  bool HasErrors = false;
  bool Initialized = false;

  // === 新管线组件 (Phase D) ===
  std::unique_ptr<frontend::FrontendBase> Frontend;
  std::unique_ptr<backend::BackendBase> Backend;
  std::unique_ptr<ir::IRContext> IRCtx;
  std::unique_ptr<ir::IRTypeContext> IRTypeCtx;
  std::unique_ptr<ir::TargetLayout> Layout;

public:
  // ... 现有公共接口不变 ...

  bool compileFile(StringRef Filename);

private:
  bool runNewPipeline(StringRef Filename);
  bool runOldPipeline(StringRef Filename);  // 提取旧管线代码
  bool runFrontend(StringRef Filename);
  bool runIRPipeline();
  bool runBackend(StringRef OutputPath);
};
```

## 测试计划

```cpp
// T1: 新管线 --frontend=cpp --backend=llvm 编译测试
// CompilerInvocation 设置 isFrontendExplicitlySet=true
// 调用 compileFile("test.cpp")
// 验证 Frontend, Backend 被创建并调用

// T2: CompilerInstance.h 无 LLVM IR 直接依赖（新管线部分）
// 新管线方法中不出现 llvm::Module, llvm::PassBuilder 等

// T3: IRModule 是唯一交互媒介
// Frontend->compile() 返回 unique_ptr<IRModule>
// Backend->emitObject() 接受 IRModule&
// 两者之间无其他数据传递
```

## 风险点

1. **IRTypeContext 生命周期**：`FrontendBase::compile()` 接受 `IRTypeContext&` 引用。`IRTypeContext` 必须在 `Frontend->compile()` 调用前创建，且生命周期需覆盖整个编译过程。
2. **TargetLayout 构建**：需确保 `TargetTriple` 非空后才创建 `TargetLayout`。
3. **BackendOptions 映射**：需从 `CompilerInvocation` 的 `CodeGenOpts` / `TargetOpts` 正确映射到 `BackendOptions`。
4. **FrontendCompileOptions 映射**：需从 `CompilerInvocation` 正确映射到 `FrontendCompileOptions`。
5. **旧管线保留**：此阶段不改旧管线代码，仅新增新管线，确保现有功能不退化。
