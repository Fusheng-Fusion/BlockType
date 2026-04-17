---
name: task-4_3_4-adl-fixes
overview: 修复 ADL (Argument-Dependent Lookup) 的四个问题：关联命名空间收集、关联类收集、友元函数查找、以及 inline 命名空间/模板特化支持。
todos:
  - id: fix-find-namespace-and-inline
    content: 重构 findNamespaceDecl 为递归搜索，添加 addAssociatedNamespace 辅助函数处理 inline 命名空间
    status: completed
  - id: fix-adl-friend-lookup
    content: 修复 LookupADL 友元函数查找，改为通过 FriendDecl 查找而非遍历所有同名函数
    status: completed
    dependencies:
      - fix-find-namespace-and-inline
  - id: fix-template-specialization-adl
    content: 在 CollectAssociatedNamespacesAndClasses 中添加 TemplateSpecializationType 处理
    status: completed
    dependencies:
      - fix-find-namespace-and-inline
  - id: build-verify-434
    content: 编译验证 Task 4.3.4 修复
    status: completed
    dependencies:
      - fix-adl-friend-lookup
      - fix-template-specialization-adl
---

## 用户需求

修复 Task 4.3.4 ADL (Argument-Dependent Lookup) 中的四个问题。

## 核心问题

1. `findNamespaceDecl` 依赖遍历父上下文的 decls 查找 NamespaceDecl，对于嵌套命名空间场景存在脆弱性
2. ADL 友元函数查找逻辑不精确 -- 当前在类 DeclContext 中查找所有同名函数，应只查找通过 FriendDecl 声明的友元函数
3. ADL 不处理模板实例化类型 (TemplateSpecializationType) -- `std::vector<int>` 的关联命名空间应包含 `std`
4. 没有处理 inline 命名空间 -- inline namespace 的成员自动暴露在外围命名空间中

## 修改范围

仅修改 `src/Sema/Lookup.cpp` 中 ADL 相关代码（第 408-562 行）。

## 技术方案

### 修改文件

`src/Sema/Lookup.cpp` -- ADL 相关三个函数及辅助函数

### 实现策略

#### 修复 1：改进 `findNamespaceDecl`

**问题**：当前实现通过遍历 `Parent->decls()` 查找 `NamespaceDecl`，仅在直接父级中搜索。虽然对当前用例基本可行，但逻辑脆弱。

**方案**：改为递归搜索所有祖先上下文的 decls，确保在任何层级都能找到。同时添加辅助函数 `addAssociatedNamespace` 封装"查找 NamespaceDecl 并插入集合 + 处理 inline 命名空间"的逻辑，减少重复代码。

#### 修复 2：ADL 友元函数查找

**问题**：当前 `LookupADL` 第 472-479 行遍历类的 `DeclContext::decls()` 查找所有同名函数，但只有 `FriendDecl` 中声明的友元函数才应通过 ADL 可见。

**方案**：遍历类的 decls 时，使用 `dyn_cast<FriendDecl>` 检查是否为 FriendDecl，然后通过 `FriendDecl::getFriendDecl()` 获取友元函数声明。只有名称匹配且为 `FunctionDecl`（非 `CXXMethodDecl`）的友元才加入结果。

#### 修复 3：TemplateSpecializationType 关联收集

**问题**：`CollectAssociatedNamespacesAndClasses` 不处理 `TemplateSpecializationType`。

**方案**：在 `CollectAssociatedNamespacesAndClasses` 中新增 `TemplateSpecializationType` 分支：

1. 添加类本身作为关联类
2. 添加外围命名空间作为关联命名空间
3. 递归处理每个 `TemplateArgument` 中类型参数的关联命名空间和类

#### 修复 4：inline 命名空间处理

**问题**：C++ inline namespace 的成员自动暴露在外围命名空间中，当前未处理。

**方案**：在 `addAssociatedNamespace` 辅助函数中，当遇到 inline namespace 时，同时将其父命名空间加入关联命名空间集合。向上遍历 inline 命名空间链，直到遇到非 inline 命名空间或 TU。

### 关键实现细节

- `NamespaceDecl` 多重继承自 `NamedDecl` 和 `DeclContext`，指针地址不同，需要通过 `static_cast<DeclContext*>(ND) == NSCtx` 比较
- `FriendDecl::getFriendDecl()` 返回 `NamedDecl*`，需要 `dyn_cast<FunctionDecl>` 获取函数声明
- `TemplateArgument::isType()` + `getAsType()` 获取类型参数
- `NamespaceDecl::isInline()` 判断是否为 inline 命名空间
- `DeclContext::getParent()` 获取父上下文

### 性能考量

- 使用现有的 `SmallPtrSet` 去重，避免重复插入
- 基类/模板参数递归深度有限，不会成为性能瓶颈
- inline 命名空间向上遍历在实际项目中层级很浅