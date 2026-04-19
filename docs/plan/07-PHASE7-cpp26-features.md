# Phase 7：C++23/C++26 特性
> **目标：** 实现 C++23/C++26 的核心新特性，包括静态反射、Contracts、Deducing this 等
> **前置依赖：** Phase 0-6 完成
> **验收标准：** 能正确编译 C++23/C++26 特性代码
> **审计日期：** 2026-04-19
> **总计：** 34 项特性 (C++23: 18 项, C++26: 16 项)
> **已实现：** 10 项 ✅ | **部分实现：** 4 项 ⚠️ | **未实现：** 20 项 ❌

---

## 📊 特性支持概览

### C++23 特性状态（8/18 已实现）

**✅ 已实现：**
- `if consteval` (P1938R3)
- 多维 `operator[]` (P2128R6)
- `#elifdef` / `#elifndef` (P2334R1)
- `#warning` 预处理指令 (P2437R1)
- Lambda 模板参数 (P1102R2)
- Lambda 属性 (P2173R1)
- `Z`/`z` 字面量后缀 (P0330R8)
- `\e` 转义序列 (P2314R4)

**⚠️ 部分实现：**
- constexpr 放宽 (P2448R2) — 部分已通过其他方式实现

**❌ 待实现（P1优先级）：**
- Deducing this / 显式对象参数 (P0847R7)
- `auto(x)` / `auto{x}` decay-copy (P0849R8)
- `static operator()` (P1169R4)
- `static operator[]` (P2589R1)
- `[[assume]]` 属性 (P1774R8)
- 占位符变量 `_` (P2169R4, C++26)
- 分隔转义 `\x{...}` (P2290R3)
- 命名转义 `\N{...}` (P2071R2)
- `for` init-statement 中 `using` (P2360R0)

### C++26 特性状态（2/16 完整 + 3 部分实现）

**✅ 已实现：**
- 包索引 `T...[N]` (P2662R3)
- `#embed` (P1967R14)

**⚠️ 部分实现：**
- 静态反射 `reflexpr` — 基础框架已建立，功能有限
- 用户 `static_assert` 消息 (P2741R3) — 支持基本格式，无格式化增强
- `@`/`$`/反引号字符集扩展 (P2558R2) — `@` 已有 token，`$` 和反引号不支持

**❌ 待实现（P1/P2优先级）：**
- Contracts (pre/post/assert) (P2900R14) — P2
- `= delete("reason")` (P2573R2) — P1
- 结构化绑定作为条件 (P0963R3) — P2
- 结构化绑定引入包 (P1061R10) — P2
- `constexpr` 异常 (P3068R6) — P2
- 平凡可重定位 (P2786R13) — P2
- 概念和变量模板模板参数 (P2841R7) — P2
- 可变参数友元 (P2893R3) — P2
- 可观察检查点 (P1494R5) — P2
- `[[indeterminate]]` 属性 (P2795R5) — P2

---

## 📌 阶段总览

```
Phase 7 包含 5 个 Stage，共 15 个 Task，预计 8 周完成。
依赖链：Stage 7.1 → Stage 7.2 → Stage 7.3 → Stage 7.4 → Stage 7.5
```

| Stage | 名称 | 核心交付物 | 建议时长 |
|-------|------|-----------|----------|
| **Stage 7.1** | C++23 P1 特性 | Deducing this、decay-copy、static operator | 2 周 |
| **Stage 7.2** | 静态反射完善 | reflexpr 完整实现、元编程支持 | 2 周 |
| **Stage 7.3** | Contracts | 前置/后置条件、断言 | 1.5 周 |
| **Stage 7.4** | C++26 P1/P2 特性 | delete 增强、占位符、结构化绑定 | 1.5 周 |
| **Stage 7.5** | 其他特性 + 测试 | 转义序列、属性、测试覆盖 | 1 周 |

---

## Stage 7.1 — C++23 P1 特性

### Task 7.1.1 Deducing this (P0847R7)

**目标：** 实现显式对象参数（Deducing this）

**开发要点：**

- **E7.1.1.1** 新增 `ExplicitObjectParam` AST 节点
- **E7.1.1.2** 解析 `this Self&& self` 语法
- **E7.1.1.3** 语义分析：推导对象类型和值类别
- **E7.1.1.4** CodeGen：生成正确的 this 指针传递

**开发关键点提示：**
> 请为 BlockType 实现 Deducing this。
>
> **语法示例**：
> ```cpp
> struct S {
>     auto foo(this const S& self);  // 显式对象参数
>     auto bar(this S&& self);       // 移动语义
> };
> ```
>
> **关键能力**：
> - 在成员函数中显式声明 this 参数
> - 支持不同的值类别（lvalue/rvalue）
> - 支持 CRTP 模式的简化

**Checkpoint：** Deducing this 正确解析和代码生成

---

### Task 7.1.2 decay-copy 表达式 (P0849R8)

**目标：** 实现 `auto(x)` 和 `auto{x}` decay-copy 语法

**开发要点：**

- **E7.1.2.1** 新增 `DecayCopyExpr` AST 节点
- **E7.1.2.2** 解析 `auto(expr)` 和 `auto{expr}` 语法
- **E7.1.2.3** 语义分析：执行 decay-copy
- **E7.1.2.4** CodeGen：生成临时对象构造代码

**开发关键点提示：**
> 请为 BlockType 实现 decay-copy。
>
> **语法示例**：
> ```cpp
> auto x = auto(expr);   // 复制初始化
> auto y = auto{expr};   // 直接初始化
> ```
>
> **语义**：
> - 对 expr 执行 decay（去除引用、cv 限定符）
> - 创建临时对象并复制或移动

**Checkpoint：** decay-copy 正确实现

---

### Task 7.1.3 static operator (P1169R4, P2589R1)

**目标：** 实现 `static operator()` 和 `static operator[]`

**开发要点：**

- **E7.1.3.1** 扩展 `DeclSpec.h` 添加 `IsStaticOperator` 标志
- **E7.1.3.2** 解析 `static operator()(...)` 语法
- **E7.1.3.3** 解析 `static operator[](args...)` 语法
- **E7.1.3.4** 语义检查：static operator 不能有 this 指针
- **E7.1.3.5** CodeGen：生成静态函数而非成员函数

**开发关键点提示：**
> 请为 BlockType 实现 static operator。
>
> **语法示例**：
> ```cpp
> struct S {
>     static int operator()(int x);      // 静态函数调用
>     static int operator[](int i, int j); // 静态下标
> };
> ```
>
> **关键约束**：
> - static operator 不能访问非静态成员
> - 没有隐式的 this 参数
> - 可以像普通静态函数一样调用

**Checkpoint：** static operator 正确实现

---

### Task 7.1.4 [[assume]] 属性 (P1774R8)

**目标：** 实现 `[[assume(condition)]]` 属性

**开发要点：**

- **E7.1.4.1** 扩展 `parseAttributeSpecifier` 识别 `assume`
- **E7.1.4.2** 新增 `AssumeAttr` AST 节点
- **E7.1.4.3** 语义检查：condition 必须是常量表达式
- **E7.1.4.4** CodeGen：生成优化提示（llvm.assume intrinsic）

**开发关键点提示：**
> 请为 BlockType 实现 [[assume]] 属性。
>
> **语法示例**：
> ```cpp
> void foo(int x) {
>     [[assume(x > 0)]];  // 告诉编译器 x 总是大于 0
>     // 编译器可以进行更激进的优化
> }
> ```
>
> **用途**：
> - 向编译器提供额外的优化信息
> - 类似 assert，但只在优化时生效
> - 不生成运行时检查代码

**Checkpoint：** [[assume]] 属性正确实现

---

## Stage 7.2 — 静态反射完善

### Task 7.2.1 reflexpr 关键字完善

**目标：** 完善 reflexpr 关键字和反射类型系统

**开发要点：**

- **E7.2.1.1** 添加 reflexpr 到词法分析器（如果尚未添加）
- **E7.2.1.2** 实现 reflexpr 表达式解析
- **E7.2.1.3** 完善反射类型系统（meta::info）
- **E7.2.1.4** 支持 reflexpr(type) 和 reflexpr(expression)

**开发关键点提示：**
> 请为 BlockType 完善静态反射。
>
> **reflexpr 关键字**：
> - 语法：`reflexpr(type)` 或 `reflexpr(expression)`
> - 返回反射类型（`meta::info`）
>
> **反射能力**：
> - 类型自省：获取类型信息
> - 成员遍历：遍历类的成员
> - 名称获取：获取类型/成员名称
> - 属性查询：查询类型属性

**Checkpoint：** reflexpr 正确解析和语义分析

---

### Task 7.2.2 元编程支持

**目标：** 实现反射的元编程能力

**开发要点：**

- **E7.2.2.1** 实现类型自省 API
- **E7.2.2.2** 实现成员遍历 API
- **E7.2.2.3** 实现元数据生成
- **E7.2.2.4** 提供编译期反射函数库

**开发关键点提示：**
> 请为 BlockType 实现反射元编程。
>
> **示例用法**：
> ```cpp
> constexpr auto info = reflexpr(MyClass);
> constexpr auto members = std::meta::get_data_members(info);
> // 编译期遍历成员
> ```

**Checkpoint：** 反射元编程正确

---

## Stage 7.3 — Contracts

### Task 7.3.1 Contract 属性 (P2900R14)

**目标：** 实现 Contract 属性（pre/post/assert）

**开发要点：**

- **E7.3.1.1** 解析 `[[pre: condition]]`、`[[post: condition]]`、`[[assert: condition]]` 属性
- **E7.3.1.2** 新增 `ContractAttr` AST 节点
- **E7.3.1.3** 语义检查 Contract 条件
- **E7.3.1.4** 生成 Contract 检查代码

**开发关键点提示：**
> 请为 BlockType 实现 Contracts。
>
> **Contract 属性**：
> - `[[pre: condition]]`：前置条件
> - `[[post: condition]]`：后置条件
> - `[[assert: condition]]`：断言
>
> **Contract 检查模式**：
> - abort：检查失败时终止
> - continue：检查失败时继续
> - observe：观察模式
> - enforce：强制检查
>
> **示例**：
> ```cpp
> int divide(int a, int b)
>     [[pre: b != 0]]
>     [[post: r >= 0]]
> {
>     return a / b;
> }
> ```

**Checkpoint：** Contracts 正确实现

---

### Task 7.3.2 Contract 语义

**目标：** 实现 Contract 的完整语义

**开发要点：**

- **E7.3.2.1** 实现 Contract 继承
- **E7.3.2.2** 实现 Contract 与虚函数的交互
- **E7.3.2.3** 实现 Contract 检查模式切换

**Checkpoint：** Contract 语义正确

---

## Stage 7.4 — C++26 P1/P2 特性

### Task 7.4.1 `= delete("reason")` (P2573R2)

**目标：** 实现带原因的 deleted 函数

**开发要点：**

- **E7.4.1.1** `FunctionDecl` 增加 `DeletedReason` 字段
- **E7.4.1.2** 解析 `= delete("reason string")` 语法
- **E7.4.1.3** 语义检查：deleted 函数不能有其他定义
- **E7.4.1.4** 诊断：使用 deleted 函数时显示原因

**开发关键点提示：**
> 请为 BlockType 实现 delete 增强。
>
> **语法示例**：
> ```cpp
> struct S {
>     void foo() = delete("Use bar() instead");
> };
> ```
>
> **用途**：
> - 提供更清晰的错误消息
> - 指导用户使用正确的 API

**Checkpoint：** delete 增强正确实现

---

### Task 7.4.2 占位符变量 `_` (P2169R4)

**目标：** 实现未命名占位符变量 `_`

**开发要点：**

- **E7.4.2.1** 词法分析器特殊处理 `_` 标识符
- **E7.4.2.2** 解析器允许 `_` 作为声明名称
- **E7.4.2.3** 语义分析：`_` 不引入新名称，每次出现都是新变量
- **E7.4.2.4** CodeGen：生成临时变量但不绑定名称

**开发关键点提示：**
> 请为 BlockType 实现占位符变量。
>
> **语法示例**：
> ```cpp
> auto [_, x] = pair;    // 忽略第一个元素
> for (auto _ : range) { }  // 循环变量不使用
> int _ = 42;            // 匿名变量
> ```
>
> **关键特性**：
> - `_` 不引入可访问的名称
> - 每次出现都是独立的变量
> - 可以用于结构化绑定、范围 for 等

**Checkpoint：** 占位符变量正确实现

---

### Task 7.4.3 结构化绑定扩展 (P0963R3, P1061R10)

**目标：** 实现结构化绑定作为条件和引入包

**开发要点：**

- **E7.4.3.1** 新增 `BindingDecl` AST 节点
- **E7.4.3.2** 支持 `if (auto [x, y] = expr)` 语法
- **E7.4.3.3** 支持 `for... [args...]` 参数包展开
- **E7.4.3.4** 语义分析：绑定变量的作用域和生命周期

**开发关键点提示：**
> 请为 BlockType 实现结构化绑定扩展。
>
> **语法示例**：
> ```cpp
> if (auto [key, value] = map.find(k)) {
>     // key 和 value 在 if 作用域内有效
> }
> 
> template<typename... Args>
> void foo(Args... args) {
>     auto [...pack] = args;  // 参数包展开
> }
> ```

**Checkpoint：** 结构化绑定扩展正确实现

---

### Task 7.4.4 Pack Indexing 完善 (P2662R3)

**目标：** 完善包索引功能（已部分实现）

**开发要点：**

- **E7.4.4.1** 验证 `PackIndexingExpr` 实现完整性
- **E7.4.4.2** 添加更多测试用例
- **E7.4.4.3** 优化代码生成

**Checkpoint：** Pack Indexing 完全正确

---

## Stage 7.5 — 其他特性 + 测试

### Task 7.5.1 转义序列扩展

**目标：** 实现分隔转义和命名转义

**开发要点：**

- **E7.5.1.1** `Lexer.cpp` 支持 `\x{HHHH...}` 分隔转义 (P2290R3)
- **E7.5.1.2** `Lexer.cpp` 支持 `\N{name}` 命名转义 (P2071R2)
- **E7.5.1.3** 需要字符名数据库（Unicode Character Database）

**开发关键点提示：**
> 请为 BlockType 实现转义序列扩展。
>
> **语法示例**：
> ```cpp
> char c1 = '\x{1F600}';  // Unicode 码点
> char c2 = '\N{LATIN CAPITAL LETTER A}';  // 字符名
> ```

**Checkpoint：** 转义序列正确解析

---

### Task 7.5.2 其他 C++26 特性

**目标：** 实现剩余的 C++26 特性

**开发要点：**

- **E7.5.2.1** `for` init-statement 中 `using` (P2360R0)
- **E7.5.2.2** `@`/`$`/反引号字符集扩展 (P2558R2)
- **E7.5.2.3** `[[indeterminate]]` 属性 (P2795R5)
- **E7.5.2.4** 平凡可重定位 (P2786R13) - 类型 trait 扩展
- **E7.5.2.5** 概念和变量模板模板参数 (P2841R7)
- **E7.5.2.6** 可变参数友元 (P2893R3)
- **E7.5.2.7** `constexpr` 异常 (P3068R6)
- **E7.5.2.8** 可观察检查点 (P1494R5)

**Checkpoint：** 其他特性正确实现

---

### Task 7.5.3 C++23/C++26 测试

**目标：** 建立 C++23/C++26 特性的完整测试覆盖

**开发要点：**

- **E7.5.3.1** 创建 `tests/cpp23/` 和 `tests/cpp26/` 目录
- **E7.5.3.2** 为每个特性编写测试用例
- **E7.5.3.3** 集成测试：组合多个特性
- **E7.5.3.4** 性能测试：确保优化有效

**测试清单：**
- [ ] Deducing this 测试
- [ ] decay-copy 测试
- [ ] static operator 测试
- [ ] [[assume]] 测试
- [ ] reflexpr 测试
- [ ] Contracts 测试
- [ ] delete("reason") 测试
- [ ] 占位符 `_` 测试
- [ ] 结构化绑定测试
- [ ] Pack Indexing 测试
- [ ] 转义序列测试
- [ ] 其他特性测试

**Checkpoint：** 测试覆盖率 ≥ 80%

---

## 📋 Phase 7 验收检查清单

### C++23 P1 特性
```
[ ] Deducing this (P0847R7) 实现完成
[ ] decay-copy (P0849R8) 实现完成
[ ] static operator() (P1169R4) 实现完成
[ ] static operator[] (P2589R1) 实现完成
[ ] [[assume]] 属性 (P1774R8) 实现完成
```

### 静态反射
```
[ ] reflexpr 关键字实现完成
[ ] 反射类型系统实现完成
[ ] 元编程支持实现完成
```

### Contracts
```
[ ] Contract 属性 (pre/post/assert) 实现完成
[ ] Contract 语义实现完成
[ ] Contract 检查模式实现完成
```

### C++26 P1/P2 特性
```
[ ] delete("reason") (P2573R2) 实现完成
[ ] 占位符变量 _ (P2169R4) 实现完成
[ ] 结构化绑定扩展 (P0963R3, P1061R10) 实现完成
[ ] Pack Indexing (P2662R3) 完善
```

### 其他特性
```
[ ] 分隔转义 \x{...} (P2290R3) 实现完成
[ ] 命名转义 \N{...} (P2071R2) 实现完成
[ ] for init-statement 中 using (P2360R0) 实现完成
[ ] @/$/反引号字符集 (P2558R2) 实现完成
[ ] [[indeterminate]] 属性 (P2795R5) 实现完成
[ ] 平凡可重定位 (P2786R13) 实现完成
[ ] 概念和变量模板模板参数 (P2841R7) 实现完成
[ ] 可变参数友元 (P2893R3) 实现完成
[ ] constexpr 异常 (P3068R6) 实现完成
[ ] 可观察检查点 (P1494R5) 实现完成
```

### 测试
```
[ ] tests/cpp23/ 目录创建
[ ] tests/cpp26/ 目录创建
[ ] 每个特性至少有一个测试用例
[ ] 集成测试通过
[ ] 性能测试通过
[ ] 测试覆盖率 ≥ 80%
```

---

*Phase 7 完成标志：能正确编译 C++23/C++26 特性代码；Deducing this、Contracts、静态反射等核心特性实现完成；测试通过，覆盖率 ≥ 80%。*

*文档更新时间：2026-04-19*
