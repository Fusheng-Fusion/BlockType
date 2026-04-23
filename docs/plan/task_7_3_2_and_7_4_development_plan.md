# Task 7.3.2 + Task 7.4 开发计划

> **规划日期：** 2026-04-23  
> **规划人：** planner  
> **执行模式：** 串行执行（planner → dev-tester → reviewer）

---

## 一、现状分析

### Task 7.3.2 Contract 语义

Task 7.3.1 已完成 95%，P0+P1 全部修复。Contract 完整链路已打通：AST → Parser → Sema → CodeGen。

**E7.3.2.1 Contract 继承：** ❌ 无任何实现。需设计继承检查算法、AST 扩展、Sema 检查、CodeGen 集成。

**E7.3.2.2 Contract 与虚函数交互：** ❌ 无实现。需确保虚调用场景下 contract 检查正确执行，纯虚函数 contract 处理。

**E7.3.2.3 Contract 检查模式切换：** ⚠️ 部分实现。`ContractMode` 枚举（5种）和 `CodeGenModule::DefaultContractMode` 已有，但无命令行入口（`-fcontract-mode=...`），无法运行时切换。

### Task 7.4.1 = delete("reason") (P2573R2)

⚠️ **大部分已实现，关键缺失：Parser 未绑定 reason 到 FunctionDecl。**

- ✅ `FunctionDecl::DeletedReason` 字段 + getter/setter（`Decl.h:212,287-293`）
- ✅ 诊断 `err_ovl_deleted_function_with_reason`（`DiagnosticSemaKinds.def:80-82`）
- ✅ Sema 重载决议中使用（`Sema.cpp:416-425`）
- ⚠️ Parser 解析了字符串但**返回值被丢弃**（`ParseDecl.cpp:2096-2097` 调用 `parsePrimaryExpression()` 未保存结果）
- ❌ 无 reason 字符串类型验证（非字符串字面量应报错）

### Task 7.4.2 占位符变量 _ (P2169R4)

⚠️ **大部分已实现，关键缺失：符号表注册、CodeGen、结构化绑定中 `_` 处理。**

- ✅ `VarDecl::IsPlaceholder` + getter/setter（`Decl.h:147,174-177`）
- ✅ `Sema::ActOnPlaceholderVarDecl`（`SemaDecl.cpp:445-455`）
- ✅ `Sema::isPlaceholderIdentifier`（`Sema.h:412-414`）
- ✅ Parser 占位符检测（`ParseDecl.cpp:1930-1974`）
- ❌ 符号表注册矛盾：注释说"不加入符号表"但 `CurContext->addDecl(VD)` 仍执行
- ❌ CodeGen 未专门处理占位符变量
- ❌ 结构化绑定中 `_` 未自动标记为占位符

### Task 7.4.3 结构化绑定扩展 (P0963R3, P1061R10)

⚠️ **核心框架已实现，关键缺失：CheckBindingCondition 空壳、while 绑定、嵌套绑定。**

- ✅ `BindingDecl` AST 节点（`Decl.h:353-380`）含 `IsPackExpansion`
- ✅ `ActOnDecompositionDecl` + `ActOnDecompositionDeclWithPack`（`SemaDecl.cpp`）
- ✅ `parseStructuredBindingDeclaration`（`ParseDecl.cpp:1818-1910`）
- ✅ if 条件中结构化绑定（`ParseStmt.cpp:578-649`）
- ✅ `CodeGen::EmitBindingDecl`（`CodeGenStmt.cpp:695-730`）
- ❌ `CheckBindingCondition` 空壳（始终返回 true，`SemaDecl.cpp:776-781`）
- ❌ while 条件中结构化绑定未实现
- ❌ 嵌套结构化绑定 `auto [[a,b],c]` 未实现
- ⚠️ P1061R10 包展开逻辑简化，无法处理变长参数包

### Task 7.4.4 Pack Indexing 完善 (P2662R3)

⚠️ **核心链路已通，需增强 CodeGen + 补充测试。**

- ✅ `PackIndexingExpr` AST 节点（`Expr.h:1456-1526`）
- ✅ Parser 解析 `T...[N]`（`ParseExpr.cpp:613-617`）
- ✅ `Sema::ActOnPackIndexingExpr`（`SemaExpr.cpp:329-388`）
- ✅ `TemplateInstantiator::InstantiatePackIndexingExpr`（`TemplateInstantiator.cpp:397-486`）
- ⚠️ `EmitPackIndexingExpr` 仅支持编译时常量索引（`CodeGenExpr.cpp:2390-2410`）
- ❌ 缺少越界/负索引/非常量索引诊断
- ⚠️ 测试覆盖不足（当前 7 个基本测试）

---

## 二、波次策略

按复杂度从低到高，分 3 个波次：

| 波次 | 任务 | 复杂度 | 预估工时 |
|------|------|--------|---------|
| **Wave 1** | Task 7.4.1 = delete("reason") 补全 | 低 | 2h |
| **Wave 1** | Task 7.4.2 占位符变量 _ 补全 | 低 | 3h |
| **Wave 2** | Task 7.4.4 Pack Indexing 完善 | 中 | 4h |
| **Wave 2** | Task 7.4.3 结构化绑定扩展 | 中 | 5h |
| **Wave 2** | Task 7.3.2.3 Contract 检查模式切换 | 中 | 3h |
| **Wave 3** | Task 7.3.2.1 Contract 继承 | 高 | 5h |
| **Wave 3** | Task 7.3.2.2 Contract 与虚函数交互 | 高 | 4h |
| | **总计** | | **26h** |

---

## 三、详细开发步骤

### Wave 1: Task 7.4.1 — = delete("reason") 补全（2h）

**步骤：**

1. **修复 Parser 绑定**（`src/Parse/ParseDecl.cpp`）
   - 保存 `parsePrimaryExpression()` 返回值为 `Expr*`
   - 验证为 `StringLiteral*`，调用 `FD->setDeletedReason()`
   - 非字符串字面量 emit 错误

2. **添加 Sema 验证**（`src/Sema/Sema.cpp`）
   - 新增 `CheckDeletedFunctionWithReason(FunctionDecl *FD)`
   - 验证 reason 仅在 `isDeleted()` 时存在

3. **新增诊断**（`DiagnosticSemaKinds.def`）
   - `err_delete_reason_not_string_literal`
   - `note_deleted_function_reason`

4. **补充测试**（`tests/cpp26/delete_with_reason.cpp`）
   - 非字符串字面量报错、空字符串、诊断输出含 reason

**修改文件：** `ParseDecl.cpp`, `Sema.cpp`, `Sema.h`, `DiagnosticSemaKinds.def`, `delete_with_reason.cpp`

---

### Wave 1: Task 7.4.2 — 占位符变量 _ 补全（3h）

**步骤：**

1. **修复符号表注册**（`src/Sema/SemaDecl.cpp`）
   - `ActOnPlaceholderVarDecl` 中移除 `CurContext->addDecl(VD)`
   - 仅注册到 CodeGen 内部结构

2. **CodeGen 占位符变量**（`src/CodeGen/CodeGenStmt.cpp`）
   - `EmitVarDecl` 中检测 `VD->isPlaceholder()`
   - 生成 alloca 使用匿名名称（`$.placeholder.N`）

3. **结构化绑定中 `_` 忽略**（`src/Sema/SemaDecl.cpp`）
   - `ActOnDecompositionDecl` 中名称为 `_` 的绑定自动标记占位符

4. **补充测试**（`tests/cpp26/placeholder_variable.cpp`）
   - 多 `_` 同作用域不冲突、`_` 不在名称查找中、结构化绑定 `_` 忽略

**修改文件：** `SemaDecl.cpp`, `CodeGenStmt.cpp`, `CGDebugInfo.cpp`, `placeholder_variable.cpp`

---

### Wave 2: Task 7.4.4 — Pack Indexing 完善（4h）

**步骤：**

1. **增强 CodeGen**（`src/CodeGen/CodeGenExpr.cpp`）
   - 非常量索引 emit 编译错误（P2662R3 要求常量索引）

2. **添加索引验证**（`src/Sema/SemaExpr.cpp`）
   - 越界/负索引/非常量索引诊断

3. **新增诊断**（`DiagnosticSemaKinds.def`）
   - `err_pack_index_out_of_bounds`
   - `err_pack_index_negative`
   - `err_pack_index_not_constant`

4. **补充测试**（`tests/cpp26/test_pack_indexing.cpp`）
   - 越界、负索引、嵌套 PackIndexing、返回类型中使用

**修改文件：** `CodeGenExpr.cpp`, `SemaExpr.cpp`, `TemplateInstantiator.cpp`, `DiagnosticSemaKinds.def`, `test_pack_indexing.cpp`

---

### Wave 2: Task 7.4.3 — 结构化绑定扩展（5h）

**步骤：**

1. **实现 CheckBindingCondition**（`src/Sema/SemaDecl.cpp`）
   - 检查绑定值可上下文转换为 bool

2. **while 条件结构化绑定**（`src/Parse/ParseStmt.cpp`）
   - 在 `parseWhileStatement` 中添加绑定检测
   - `WhileStmt` 添加 `BindingDecls` 字段（`Stmt.h`）

3. **CodeGen while 绑定**（`src/CodeGen/CodeGenStmt.cpp`）
   - `EmitWhileStmt` 中处理 BindingDecls

4. **P1061R10 包展开完善**（`src/Sema/SemaDecl.cpp`）
   - 处理变长参数包，根据 tuple 大小确定展开数量

5. **补充测试**
   - while 绑定、CheckBindingCondition 错误、包展开

**修改文件：** `SemaDecl.cpp`, `ParseStmt.cpp`, `Stmt.h`, `CodeGenStmt.cpp`, `DiagnosticSemaKinds.def`, `structured_binding_extensions.cpp`

---

### Wave 2: Task 7.3.2.3 — Contract 检查模式切换（3h）

**步骤：**

1. **添加编译选项**（`CompilerInvocation.h`）
   - `ContractMode DefaultContractMode = ContractMode::Enforce`

2. **命令行解析**（`CompilerInvocation.cpp`）
   - `-fcontract-mode=off|default|enforce|observe|quick_enforce`
   - `-fno-contracts` / `-fcontracts` 简写

3. **传播到 CodeGenModule**（`CodeGenModule.cpp`）
   - 从 Invocation 读取并设置默认模式

4. **补充测试**（`tests/lit/CodeGen/cpp26-contracts.test`）
   - 各模式切换测试

**修改文件：** `CompilerInvocation.h`, `CompilerInvocation.cpp`, `CodeGenModule.cpp`, `cpp26-contracts.test`

---

### Wave 3: Task 7.3.2.1 — Contract 继承（5h）

**步骤：**

1. **设计继承规则**
   - Precondition：派生类可放宽（OR 语义合并）
   - Postcondition：派生类可加强（AND 语义合并）
   - Assert 不继承

2. **AST 扩展**（`Decl.h`）
   - `FunctionDecl` 添加 `InheritedContracts` 字段

3. **Sema 继承检查**（`SemaCXX.cpp`）
   - 新增 `CheckOverridingFunctionContracts(Derived, Base)`
   - 在虚函数覆盖时调用

4. **CodeGen 继承 Contract**（`CodeGenFunction.cpp`）
   - `EmitFunctionBody` 同时检查继承和自身 contracts

5. **新增诊断 + 测试**

**修改文件：** `Decl.h`, `SemaCXX.h`, `SemaCXX.cpp`, `Sema.cpp`, `DiagnosticSemaKinds.def`, `CodeGenFunction.cpp`, `test_contracts.cpp`

---

### Wave 3: Task 7.3.2.2 — Contract 与虚函数交互（4h）

**步骤：**

1. **虚调用场景验证**
   - 确认虚函数最终执行自身的 Contract 检查（含继承的）
   - vtable 不受 Contract 影响

2. **纯虚函数 Contract**（`SemaCXX.cpp`）
   - 纯虚函数可有 Contract，作为接口契约
   - 派生类必须满足纯虚函数的 Contract

3. **补充测试**
   - 基类指针调用派生类虚函数、纯虚函数 Contract

**修改文件：** `SemaCXX.cpp`, `CodeGenCXX.cpp`, `test_contracts.cpp`

---

## 四、依赖关系

```
Wave 1（无依赖）
├── 7.4.1 = delete("reason") 补全
└── 7.4.2 占位符变量 _ 补全

Wave 2（依赖 Wave 1 稳定性）
├── 7.4.4 Pack Indexing 完善
├── 7.4.3 结构化绑定扩展（依赖 7.4.2 的 `_` 占位符机制）
└── 7.3.2.3 Contract 检查模式切换

Wave 3（依赖 7.3.2.3）
├── 7.3.2.1 Contract 继承（依赖 7.3.2.3 模式切换）
└── 7.3.2.2 Contract 与虚函数交互（依赖 7.3.2.1 继承）
```

---

## 五、测试策略

| 任务 | 最低测试数 | 关键场景 |
|------|-----------|---------|
| 7.4.1 | 5 | 非字符串报错、空字符串、诊断含 reason |
| 7.4.2 | 6 | 多 `_` 不冲突、符号表隔离、结构化绑定 `_` |
| 7.4.4 | 10 | 越界/负索引/非常量报错、嵌套、返回类型 |
| 7.4.3 | 8 | if/while 条件、CheckBindingCondition、包展开 |
| 7.3.2.3 | 4 | 各模式切换 |
| 7.3.2.1 | 6 | 继承合并、诊断、多层继承 |
| 7.3.2.2 | 4 | 虚调用、纯虚函数 Contract |
