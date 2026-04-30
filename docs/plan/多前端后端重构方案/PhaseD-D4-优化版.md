# Phase D.4 — 自动注册机制（优化版）

## 原始 Spec 问题

### P1 — 必须修复
1. **`CppFrontendRegistration.cpp` 已存在**：
   - 文件 `src/Frontend/CppFrontendRegistration.cpp` 已经实现了 CppFrontend 的静态注册。
   - Spec 说"新增 `src/Frontend/CppFrontendRegistration.cpp`"，这是错误的。
   - **修复**：此任务仅需**新增** `src/Backend/LLVMBackendRegistration.cpp`。

2. **Spec 的注册代码使用了错误的命名空间**：
   - Spec 写 `FrontendRegistry::instance()` 但实际代码使用 `frontend::FrontendRegistry::instance()`。
   - Spec 写 `BackendRegistry::instance()` 但实际代码使用 `backend::BackendRegistry::instance()`。
   - **修复**：使用完整的命名空间限定名。

### P2 — 建议修复
1. **Spec 未提及 `createCppFrontend` 和 `createLLVMBackend` 函数的声明位置**：
   - `createCppFrontend` 已在 `CppFrontendRegistration.cpp` 中定义（static linkage）。
   - `createLLVMBackend` 已在 `src/Backend/LLVMBackend.cpp` 中定义（extern linkage，在 `LLVMBackend.h` 中声明）。
   - **修复**：注册代码直接调用已有的工厂函数。

2. **静态初始化顺序问题**：全局单例依赖静态初始化，需确保 `FrontendRegistry::instance()` / `BackendRegistry::instance()` 在 Registrator 构造时可用（Meyers' Singleton 保证这一点）。

### P3 — 建议改进
1. 考虑使用 `llvm::ManagedStatic` 替代手工单例，解决静态析构顺序问题。

## 依赖分析

- **前置依赖**：D.2（CompilerInstance 重构）
- **已满足**：`CppFrontendRegistration.cpp` 已存在

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `src/Backend/LLVMBackendRegistration.cpp` |

**注意**：`src/Frontend/CppFrontendRegistration.cpp` **已存在**，不需要创建或修改。

## 当前代码状态

- `src/Frontend/CppFrontendRegistration.cpp` 已实现 CppFrontend 注册，包含 `.cpp`, `.cc`, `.cxx`, `.c` 扩展名映射。
- `LLVMBackend.h` 已声明 `createLLVMBackend()` 工厂函数。
- `LLVMBackend.cpp` 已实现 `createLLVMBackend()`。
- **缺失**：没有 `LLVMBackendRegistration.cpp`，LLVM 后端未通过静态初始化自动注册到 `BackendRegistry`。

## 实现步骤

1. **创建 `src/Backend/LLVMBackendRegistration.cpp`**：
   ```cpp
   #include "blocktype/Backend/BackendRegistry.h"
   #include "blocktype/Backend/LLVMBackend.h"

   namespace blocktype::backend {

   static struct LLVMBackendRegistrator {
     LLVMBackendRegistrator() {
       BackendRegistry::instance().registerBackend("llvm", createLLVMBackend);
     }
   } LLVMBackendRegistratorInstance;

   } // namespace blocktype::backend
   ```

2. **确保 `LLVMBackendRegistration.cpp` 被编译链接**：
   - 在 `CMakeLists.txt` 中将 `src/Backend/LLVMBackendRegistration.cpp` 添加到 `libblocktype-backend-llvm` 的源文件列表中。
   - 使用链接时确保该编译单元不被优化掉（可能需要 `-Wl,--whole-archive` 或编译器保留机制）。

3. **验证注册成功**：
   - 在测试中检查 `backend::BackendRegistry::instance().hasBackend("llvm") == true`。

## 接口签名（最终版）

```cpp
// src/Backend/LLVMBackendRegistration.cpp
namespace blocktype::backend {

static struct LLVMBackendRegistrator {
  LLVMBackendRegistrator() {
    BackendRegistry::instance().registerBackend("llvm", createLLVMBackend);
  }
} LLVMBackendRegistratorInstance;

} // namespace blocktype::backend
```

## 测试计划

```cpp
// T1: 后端注册后可查找
assert(backend::BackendRegistry::instance().hasBackend("llvm") == true);

// T2: 前端注册后可查找
assert(frontend::FrontendRegistry::instance().hasFrontend("cpp") == true);

// T3: 创建后端实例
auto BE = backend::BackendRegistry::instance().create("llvm", Opts, Diags);
assert(BE != nullptr);
assert(BE->getName() == "llvm");

// T4: 多 CompilerInstance 实例不冲突
{
  CompilerInstance CI1;
  CompilerInstance CI2;
  // 两个实例各自独立创建 Frontend/Backend
}
```

## 风险点

1. **静态初始化顺序**：`LLVMBackendRegistratorInstance` 的构造依赖 `BackendRegistry::instance()`，后者是 Meyers' Singleton（首次访问时构造）。只要 `LLVMBackendRegistration.cpp` 被链接且不被优化掉，就能保证正确初始化。
2. **链接优化**：某些链接器会移除"未使用"的静态对象。确保 `LLVMBackendRegistration.o` 被保留。
3. **重复注册**：如果多个编译单元都注册 "llvm"，`BackendRegistry` 应能检测并报错（当前实现使用 assert）。
