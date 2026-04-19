---
name: diagnostics-migration-p1-p5
overview: 修复诊断系统关键功能缺失：语句上下文检查、符号重定义、void表达式检查、未使用声明警告、隐式转换警告。覆盖 12 个已定义但从未使用的诊断 ID。
todos:
  - id: p1-stmt-context
    content: "P1: 添加 BreakScopeDepth/ContinueScopeDepth/SwitchScopeDepth + 修复 break/continue/case/default 检查"
    status: completed
  - id: p1-test
    content: "P1: 编译 + 全量测试验证"
    status: completed
    dependencies:
      - p1-stmt-context
  - id: p2-redef
    content: "P2: SymbolTable 注入 Diags + 重定义诊断"
    status: completed
    dependencies:
      - p1-test
  - id: p2-test
    content: "P2: 编译 + 全量测试验证"
    status: completed
    dependencies:
      - p2-redef
  - id: p3-void-check
    content: "P3: void 表达式检查（binary/conditional/unary）"
    status: completed
    dependencies:
      - p2-test
  - id: p3-test
    content: "P3: 编译 + 全量测试验证"
    status: completed
    dependencies:
      - p3-void-check
  - id: p4-unused
    content: "P4: Decl::Used 标志 + DeclRefExpr 标记 + 不可达代码 + DiagnoseUnusedDecls"
    status: completed
    dependencies:
      - p3-test
  - id: p4-test
    content: "P4: 编译 + 全量测试验证"
    status: completed
    dependencies:
      - p4-unused
  - id: p5-call-diag
    content: "P5: 新增诊断 ID + CheckCall 精确诊断 + 隐式转换警告"
    status: completed
    dependencies:
      - p4-test
  - id: p5-test
    content: "P5: 编译 + 全量测试验证"
    status: completed
    dependencies:
      - p5-call-diag
  - id: commit
    content: 提交 git commit + 更新开发文档
    status: completed
    dependencies:
      - p5-test
---

## 产品概述

修复 AST 重构后诊断系统缺失的 5 项关键功能，恢复已定义但未使用的 12+ 个诊断 ID。

## 核心功能

### P1: 语句上下文检查（影响正确性）

- `ActOnBreakStmt` 检查是否在循环或 switch 中，否则发出 `err_break_outside_loop`
- `ActOnContinueStmt` 检查是否在循环中，否则发出 `err_continue_outside_loop`
- `ActOnCaseStmt` / `ActOnDefaultStmt` 检查是否在 switch 中，否则发出 `err_case_not_in_switch`
- Sema 添加 `BreakScopeDepth` / `ContinueScopeDepth` 成员，在循环/switch 入口递增、出口递减

### P2: 符号重定义检查（影响正确性）

- `SymbolTable::addOrdinarySymbol` 检测到非函数重定义时，发出 `err_redefinition` + `note_previous_definition`
- `SymbolTable::addTagDecl` 检测到类/结构体重定义时，发出同样诊断
- SymbolTable 获取 `DiagnosticsEngine&` 引用

### P3: void 表达式检查（影响可用性）

- `ActOnBinaryOperator` 检查 void 操作数，发出 `err_void_expr_not_allowed`
- `ActOnConditionalExpr` 检查 void 条件分支
- `ActOnReturnStmt` 检查非 void 函数返回 void 表达式

### P4: 未使用声明警告（质量改进）

- 在 `Decl` 基类添加 `Used` 标志
- `ActOnDeclRefExpr` 创建时标记 `ValueDecl::setUsed()`
- 新增 `Sema::DiagnoseUnusedDecls()` 方法，编译末期遍历 AST 发出 `warn_unused_variable` / `warn_unused_function`
- `ActOnCompoundStmt` / `ActOnReturnStmt` 检测不可达代码，发出 `warn_unreachable_code`

### P5: 诊断精确化 + 函数调用参数诊断增强（质量改进）

- `CheckCall` 参数数量不匹配时，发出具体的 `err_too_few_args`/`err_too_many_args` 诊断（需新增）
- `CheckAssignment` 发生隐式窄化转换时，发出 `warn_implicit_conversion`
- **替换 `err_type_mismatch` 泛用**：将 Sema 中 ~15 处 `err_type_mismatch` 替换为具体诊断（需新增 5 个诊断 ID）

## 技术栈

C++ 编译器项目（BlockType），LLVM 基础设施，CMake 构建系统。修改涉及 Sema 层和诊断定义。

## 实现方案

### P1: 语句上下文检查 — 深度计数器方案

**原理**：在 Sema 中维护两个无符号计数器，循环/switch 进入时递增、退出时递减。`ActOnBreakStmt` 等方法检查计数器 > 0。

**新增成员**（Sema.h）：

```cpp
unsigned BreakScopeDepth = 0;     // loop + switch 都递增
unsigned ContinueScopeDepth = 0;  // 仅 loop 递增
unsigned SwitchScopeDepth = 0;    // 仅 switch 递增
```

**修改方法**：

- `ActOnWhileStmt`/`ActOnForStmt`/`ActOnDoStmt`：方法体首尾递增/递减 `BreakScopeDepth` + `ContinueScopeDepth`
- `ActOnSwitchStmt`：递增/递减 `BreakScopeDepth` + `SwitchScopeDepth`
- `ActOnBreakStmt`：检查 `BreakScopeDepth > 0`
- `ActOnContinueStmt`：检查 `ContinueScopeDepth > 0`
- `ActOnCaseStmt`/`ActOnDefaultStmt`：检查 `SwitchScopeDepth > 0`

**注意**：Body 已经由 Parser 在调用 ActOn* 之前解析完毕，所以递增/递减应包裹整个方法体。递减需在 return 之前，包括错误路径。

### P2: 符号重定义 — SymbolTable 注入 Diags

**原理**：SymbolTable 当前只有 `ASTContext&`，需要添加 `DiagnosticsEngine&`。

**修改**：

- `SymbolTable.h`：构造函数改为 `SymbolTable(ASTContext &C, DiagnosticsEngine &D)`，添加成员 `DiagnosticsEngine &Diags`
- `Sema.cpp`：初始化列表改为 `Symbols(C, D)`
- `SymbolTable.cpp`：`addOrdinarySymbol` 在非函数重定义时发 `err_redefinition` + `note_previous_definition`
- `addTagDecl`：在非前向声明重定义时发同样诊断

### P3: void 表达式检查

**原理**：void 类型的表达式不应参与算术运算或作为条件。

**修改位置**（均在 Sema.cpp）：

- `ActOnBinaryOperator`：在类型检查前检查 LHS/RHS 是否为 void，若是则发 `err_void_expr_not_allowed`
- `ActOnConditionalExpr`：Then/Else 为 void 时检查
- `ActOnUnaryOperator`（Plus/Minus/Not/Inc/Dec）：操作数为 void 时发出

### P4: 未使用声明 + 不可达代码

**Decl 基类修改**（Decl.h）：

```cpp
class Decl : public ASTNode {
protected:
  bool Used = false;
  Decl(SourceLocation Loc) : ASTNode(Loc) {}
public:
  bool isUsed() const { return Used; }
  void setUsed(bool U = true) { Used = U; }
};
```

**Sema.cpp 修改**：

- `ActOnDeclRefExpr`：创建 DRE 后调用 `D->setUsed()`
- `DiagnoseUnusedDecls()`：遍历 CurTU 所有顶层声明 + 函数体内的局部声明
- `ActOnReturnStmt`：之后的语句标记不可达
- 诊断位置：在 driver.cpp 编译末期调用 `S.DiagnoseUnusedDecls()`

**不可达代码检测**：在 `ActOnCompoundStmt` 中，如果发现 `ReturnStmt` 后还有语句，发 `warn_unreachable_code`。

### P5: 函数调用诊断增强

**新增诊断 ID**（DiagnosticSemaKinds.def）：

```
// P5-a: 函数调用参数精确诊断
DIAG(err_too_few_args, Error, "too few arguments to function call, expected %0, have %1", ...)
DIAG(err_too_many_args, Error, "too many arguments to function call, expected %0, have %1", ...)
DIAG(err_arg_type_mismatch, Error, "cannot convert argument %0 of type '%1' to parameter type '%2'", ...)

// P5-b: 替代 err_type_mismatch 的精确诊断
DIAG(err_bin_op_type_invalid, Error, "invalid operands to binary expression ('%0' and '%1')", ...)
DIAG(err_condition_not_bool, Error, "expression is not contextually convertible to bool", ...)
DIAG(err_subscript_not_pointer, Error, "subscript of type '%0' is not a pointer or array", ...)
DIAG(err_cast_incompatible, Error, "cannot cast from type '%0' to type '%1'", ...)
DIAG(err_member_access_type_invalid, Error, "member access into type '%0', which is not a record", ...)
DIAG(err_invalid_pointer_arithmetic, Error, "invalid operands to pointer arithmetic ('%0' and '%1')", ...)
```

**TypeCheck.cpp 修改**：

- `CheckCall`：参数数量不足发 `err_too_few_args`，过多发 `err_too_many_args`
- `CheckCall`：单个参数类型不匹配发 `err_arg_type_mismatch`
- `CheckInitialization`/`CheckAssignment`：当发生隐式转换时，检查是否窄化（float→int, long→int 等），发 `warn_implicit_conversion`

**Sema.cpp 修改（替代 err_type_mismatch）**：

| 当前位置 | 当前诊断 | 替换为 |
| --- | --- | --- |
| `ActOnBinaryOperator` 类型推导失败 | `err_type_mismatch` | `err_bin_op_type_invalid`（含 LHS/RHS 类型名） |
| `ActOnConditionalExpr` 类型不兼容 | `err_type_mismatch` | `err_bin_op_type_invalid`（含 Then/Else 类型名） |
| `ActOnArraySubscriptExpr` 非 ptr/array | `err_type_mismatch` | `err_subscript_not_pointer`（含 base 类型名） |
| `ActOnArraySubscriptExpr` index 非 int | `err_type_mismatch` | `err_bin_op_type_invalid` |
| `ActOnCastExpr` 类型不兼容 | `err_type_mismatch` | `err_cast_incompatible`（含源/目标类型名） |
| `ActOnMemberExpr`/`ActOnMemberExprDirect` base 非记录类型 | `err_type_mismatch` | `err_member_access_type_invalid` |
| `ActOnSwitchStmt` cond 非整数 | `err_type_mismatch` | `err_condition_not_bool` |
| `TypeCheck::CheckCondition` 非可转换 | `err_type_mismatch` | `err_condition_not_bool` |


## 目录结构

```
project-root/
├── include/blocktype/
│   ├── AST/Decl.h                    # [MODIFY] 添加 Used 标志 + isUsed/setUsed
│   ├── Sema/Sema.h                   # [MODIFY] 添加 BreakScopeDepth/ContinueScopeDepth/SwitchScopeDepth, DiagnoseUnusedDecls()
│   ├── Sema/SymbolTable.h            # [MODIFY] 构造函数添加 DiagnosticsEngine&
│   └── Basic/DiagnosticSemaKinds.def # [MODIFY] 新增 8 个精确诊断 ID
├── src/Sema/
│   ├── Sema.cpp                      # [MODIFY] P1 上下文检查 + P3 void 检查 + P4 DeclRefExpr.setUsed + DiagnoseUnusedDecls + 不可达代码 + P5 err_type_mismatch 精确化
│   ├── SymbolTable.cpp               # [MODIFY] P2 重定义诊断
│   └── TypeCheck.cpp                 # [MODIFY] P5 调用参数诊断 + 隐式转换警告
└── tools/driver.cpp                  # [MODIFY] P4 编译末期调用 DiagnoseUnusedDecls
```

## 实现注意事项

- P1 深度计数器必须在所有 return 路径上递减（包括错误返回），否则后续语句会误判。用 RAII guard 最安全，但为简化可直接在每个 return 前递减。
- P2 SymbolTable 修改后，`addOrdinarySymbol` 仍然返回 bool（true 表示成功），但重定义时仍添加到表中（允许继续编译）同时发出诊断。
- P3 void 检查应在类型检查逻辑之前执行，避免 void 操作数参与 `getCommonType` 等导致意外结果。
- P4 `DiagnoseUnusedDecls` 需要遍历 AST — 不能复用已删除的 ProcessAST（ASTVisitor），需要写一个简单的遍历。但不同于 ProcessAST 遍历所有表达式，这里只需遍历 Stmt 级别检查不可达代码。
- P5 隐式转换警告需要判断是否有损：float→int 是有损的，int→long 是安全的。利用已有的 `isTypeCompatible` + 新增 `isNarrowingConversion` 辅助方法。
- P5 `err_type_mismatch` 精确化：保留 `err_type_mismatch` 不删除（TypeCheck 中的初始化/赋值检查仍用），只在 Sema.cpp 的 ActOn* 方法中替换为更具体的诊断。需要利用已有的类型打印（`QualType::getAsString()` 或类似方法）来填充 `%0`/`%1` 参数。