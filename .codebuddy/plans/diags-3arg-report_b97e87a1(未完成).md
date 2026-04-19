---
name: diags-3arg-report
overview: DiagnosticsEngine 添加 3 参数 report 重载，恢复 err_arg_type_mismatch 为完整 3 参数格式。
todos:
  - id: add-3arg-report
    content: 在 Diagnostics.h 和 Diagnostics.cpp 中添加 3 参数 report 重载
    status: pending
  - id: restore-arg-mismatch
    content: 恢复 err_arg_type_mismatch 为 3 参数格式 + 更新 TypeCheck.cpp 调用
    status: pending
    dependencies:
      - add-3arg-report
  - id: build-test
    content: 编译 + 662 全量测试验证
    status: pending
    dependencies:
      - restore-arg-mismatch
---

## 用户需求

DiagnosticsEngine 的 `report` 方法当前只支持 0/1/2 个替换参数，需要添加 3 参数版本（Arg0, Arg1, Arg2），并恢复 `err_arg_type_mismatch` 诊断为完整的 `%0`/`%1`/`%2` 三参数格式。

## 修改范围

共 4 个文件，每个文件改动极小（1-6 行）。

## 技术栈

C++ 编译器项目（BlockType），LLVM 基础设施。

## 实现方案

完全复用现有 2 参数 `report` 的模式，扩展为 3 参数版本。`formatMessage` 已支持任意数量参数（`ArrayRef<StringRef>`），无需修改。

### 修改清单

| 文件 | 修改 |
| --- | --- |
| `include/blocktype/Basic/Diagnostics.h` | 在 2 参数 report 声明后添加 3 参数声明 |
| `src/Basic/Diagnostics.cpp` | 在 2 参数实现后添加 3 参数实现 |
| `include/blocktype/Basic/DiagnosticSemaKinds.def` | 恢复 `err_arg_type_mismatch` 为完整 3 参数消息 |
| `src/Sema/TypeCheck.cpp` | `CheckCall` 传入 3 个参数（argNum, argType, paramType） |