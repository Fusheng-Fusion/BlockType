# AI Coder 可执行任务流 — Phase B 增强任务 B-F7：DiagnosticGroupManager 扩展

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype::diag` 中
5. **头文件路径**：`include/blocktype/IR/`，源文件路径：`src/IR/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F7 — DiagnosticGroupManager 扩展"
   git push origin HEAD
   ```

---

## Task B-F7：DiagnosticGroupManager 扩展

### 依赖验证

✅ **A-F6（StructuredDiagnostic 基础结构）**：已存在于 `include/blocktype/IR/IRDiagnostic.h`
✅ **DiagnosticGroupManager 基础实现**：已存在于 `src/IR/IRDiagnostic.cpp`（第 146-183 行）

### 现状分析

**DiagnosticGroupManager 已有基础实现**（Pimpl 模式），包含：
- `enableGroup()` / `disableGroup()` / `isGroupEnabled()`
- `enableAll()` / `disableAll()`

**本任务需要扩展**：
1. 添加命令行选项解析支持
2. 添加诊断组列表查询接口
3. 添加诊断组与诊断码映射

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `include/blocktype/IR/IRDiagnostic.h` |
| 修改 | `src/IR/IRDiagnostic.cpp` |

---

## 必须实现的扩展

### 1. 添加诊断组列表查询接口

```cpp
// 在 IRDiagnostic.cpp 中实现

namespace blocktype {
namespace diag {

class DiagnosticGroupManager::Impl {
  DenseMap<DiagnosticGroup, bool> EnabledGroups;
  
public:
  Impl() {
    // 默认启用所有组
    enableAll();
  }
  
  void enableGroup(DiagnosticGroup G) {
    EnabledGroups[G] = true;
  }
  
  void disableGroup(DiagnosticGroup G) {
    EnabledGroups[G] = false;
  }
  
  bool isGroupEnabled(DiagnosticGroup G) const {
    auto It = EnabledGroups.find(G);
    return It != EnabledGroups.end() && It->second;
  }
  
  void enableAll() {
    // 启用所有已知组
    EnabledGroups[DiagnosticGroup::TypeMapping] = true;
    EnabledGroups[DiagnosticGroup::InstructionValidation] = true;
    EnabledGroups[DiagnosticGroup::IRVerification] = true;
    EnabledGroups[DiagnosticGroup::BackendCodegen] = true;
    EnabledGroups[DiagnosticGroup::FFIBinding] = true;
    EnabledGroups[DiagnosticGroup::Serialization] = true;
  }
  
  void disableAll() {
    for (auto& KV : EnabledGroups) {
      KV.second = false;
    }
  }
};

// DiagnosticGroupManager 实现（Pimpl 模式）

DiagnosticGroupManager::DiagnosticGroupManager()
  : Pimpl(new Impl()) {}

DiagnosticGroupManager::~DiagnosticGroupManager() {
  delete Pimpl;
}

void DiagnosticGroupManager::enableGroup(DiagnosticGroup G) {
  Pimpl->enableGroup(G);
}

void DiagnosticGroupManager::disableGroup(DiagnosticGroup G) {
  Pimpl->disableGroup(G);
}

bool DiagnosticGroupManager::isGroupEnabled(DiagnosticGroup G) const {
  return Pimpl->isGroupEnabled(G);
}

void DiagnosticGroupManager::enableAll() {
  Pimpl->enableAll();
}

void DiagnosticGroupManager::disableAll() {
  Pimpl->disableAll();
}

} // namespace diag
} // namespace blocktype
```

---

## 诊断分组映射表

| DiagnosticGroup | 包含的诊断码 | 说明 |
|----------------|-------------|------|
| TypeMapping | TypeMappingFailed, TypeMappingAmbiguous | 类型映射错误 |
| InstructionValidation | InvalidInstruction, InvalidOperand | 指令验证错误 |
| IRVerification | VerificationFailed, InvalidModule | IR 验证错误 |
| BackendCodegen | CodegenFailed, UnsupportedType | 后端代码生成错误 |
| FFIBinding | FFIBindingFailed, InvalidFFISignature | FFI 绑定错误 |
| Serialization | SerializationFailed, DeserializationFailed | 序列化错误 |

---

## 编译选项支持

```cpp
// 在 CompilerInvocation 中添加选项解析

// -W<group> 启用诊断组
if (Arg *A = Args.getLastArg(OPT_W_Group)) {
  StringRef GroupName = A->getValue();
  DiagnosticGroup G = parseDiagnosticGroup(GroupName);
  DiagGroupManager.enableGroup(G);
}

// -Wno-<group> 禁用诊断组
if (Arg *A = Args.getLastArg(OPT_Wno_Group)) {
  StringRef GroupName = A->getValue();
  DiagnosticGroup G = parseDiagnosticGroup(GroupName);
  DiagGroupManager.disableGroup(G);
}

// -Wall 启用所有诊断组
if (Args.hasArg(OPT_Wall)) {
  DiagGroupManager.enableAll();
}

// -Wextra 启用额外诊断组（同 -Wall）
if (Args.hasArg(OPT_Wextra)) {
  DiagGroupManager.enableAll();
}

// -Werror 将所有警告视为错误
if (Args.hasArg(OPT_Werror)) {
  DiagEngine.setWarningsAsErrors(true);
}

// -Wir 启用 IR 相关诊断
if (Args.hasArg(OPT_Wir)) {
  DiagGroupManager.enableGroup(DiagnosticGroup::IRVerification);
  DiagGroupManager.enableGroup(DiagnosticGroup::TypeMapping);
}

// -Wdialect 启用 Dialect 相关诊断
if (Args.hasArg(OPT_Wdialect)) {
  DiagGroupManager.enableGroup(DiagnosticGroup::InstructionValidation);
}
```

### 辅助函数

```cpp
// 在 IRDiagnostic.cpp 中添加

DiagnosticGroup parseDiagnosticGroup(StringRef Name) {
  return StringSwitch<DiagnosticGroup>(Name)
    .Case("type-mapping", DiagnosticGroup::TypeMapping)
    .Case("instruction-validation", DiagnosticGroup::InstructionValidation)
    .Case("ir-verification", DiagnosticGroup::IRVerification)
    .Case("backend-codegen", DiagnosticGroup::BackendCodegen)
    .Case("ffi-binding", DiagnosticGroup::FFIBinding)
    .Case("serialization", DiagnosticGroup::Serialization)
    .Default(DiagnosticGroup::TypeMapping);  // 默认
}
```

---

## 与 DiagnosticsEngine 集成

```cpp
// 在 DiagnosticsEngine 中添加过滤逻辑

bool DiagnosticsEngine::shouldEmitDiagnostic(DiagnosticCode Code) const {
  DiagnosticGroup G = getGroupForCode(Code);
  return DiagGroupManager.isGroupEnabled(G);
}

void DiagnosticsEngine::emitDiagnostic(const StructuredDiagnostic& D) {
  if (!shouldEmitDiagnostic(D.getCode())) {
    return;  // 诊断组被禁用，跳过
  }
  
  // 发射诊断
  if (Emitter) {
    Emitter->emit(D);
  }
}
```

---

## 实现约束

1. **Pimpl 模式**：隐藏 DenseMap 实现，避免头文件暴露
2. **默认启用**：所有诊断组默认启用（除非显式禁用）
3. **线程安全**：DiagnosticGroupManager 仅在编译初始化阶段修改，编译期间只读
4. **零开销**：禁用的诊断组在编译期间不产生字符串格式化开销

---

## 验收标准

### 单元测试

```cpp
// tests/unit/IR/DiagnosticGroupManagerTest.cpp

#include "gtest/gtest.h"
#include "blocktype/IR/IRDiagnostic.h"

using namespace blocktype;

TEST(DiagnosticGroupManager, EnableDisable) {
  diag::DiagnosticGroupManager Mgr;
  
  // 默认启用
  EXPECT_TRUE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  
  // 禁用
  Mgr.disableGroup(diag::DiagnosticGroup::TypeMapping);
  EXPECT_FALSE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  
  // 重新启用
  Mgr.enableGroup(diag::DiagnosticGroup::TypeMapping);
  EXPECT_TRUE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
}

TEST(DiagnosticGroupManager, EnableAll) {
  diag::DiagnosticGroupManager Mgr;
  
  Mgr.disableAll();
  EXPECT_FALSE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  EXPECT_FALSE(Mgr.isGroupEnabled(diag::DiagnosticGroup::IRVerification));
  
  Mgr.enableAll();
  EXPECT_TRUE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  EXPECT_TRUE(Mgr.isGroupEnabled(diag::DiagnosticGroup::IRVerification));
}

TEST(DiagnosticGroupManager, DisableAll) {
  diag::DiagnosticGroupManager Mgr;
  
  Mgr.enableAll();
  EXPECT_TRUE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  
  Mgr.disableAll();
  EXPECT_FALSE(Mgr.isGroupEnabled(diag::DiagnosticGroup::TypeMapping));
  EXPECT_FALSE(Mgr.isGroupEnabled(diag::DiagnosticGroup::IRVerification));
}

TEST(DiagnosticGroupManager, ParseGroup) {
  using namespace diag;
  
  EXPECT_EQ(parseDiagnosticGroup("type-mapping"), DiagnosticGroup::TypeMapping);
  EXPECT_EQ(parseDiagnosticGroup("ir-verification"), DiagnosticGroup::IRVerification);
  EXPECT_EQ(parseDiagnosticGroup("backend-codegen"), DiagnosticGroup::BackendCodegen);
}

TEST(DiagnosticGroupManager, GetGroupForCode) {
  using namespace diag;
  
  EXPECT_EQ(getGroupForCode(DiagnosticCode::TypeMappingFailed), 
            DiagnosticGroup::TypeMapping);
  EXPECT_EQ(getGroupForCode(DiagnosticCode::VerificationFailed), 
            DiagnosticGroup::IRVerification);
  EXPECT_EQ(getGroupForCode(DiagnosticCode::CodegenFailed), 
            DiagnosticGroup::BackendCodegen);
}
```

### 集成测试

```bash
# V1: 禁用诊断组
# blocktype -Wno-type-mapping test.cpp 2>&1 | grep -c "TypeMappingFailed"
# 输出: 0

# V2: 启用诊断组
# blocktype -Wtype-mapping test.cpp 2>&1 | grep "TypeMappingFailed"
# 输出包含诊断

# V3: -Wall 启用所有
# blocktype -Wall test.cpp 2>&1 | grep -c "error"
# 输出 > 0

# V4: -Werror 将警告视为错误
# blocktype -Werror test.cpp
# 退出码 != 0（如果有警告）

# V5: -Wir 启用 IR 相关诊断
# blocktype -Wir test.cpp 2>&1 | grep "IR"
# 输出包含 IR 相关诊断

# V6: -Wdialect 启用 Dialect 相关诊断
# blocktype -Wdialect test.cpp 2>&1 | grep "dialect"
# 输出包含 Dialect 相关诊断
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F7 — DiagnosticGroupManager 基础实现" && git push origin HEAD
```
