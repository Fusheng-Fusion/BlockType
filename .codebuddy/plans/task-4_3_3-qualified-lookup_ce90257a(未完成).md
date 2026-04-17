---
name: task-4_3_3-qualified-lookup
overview: 修复 Qualified Lookup 中的基类查找缺失问题。TypeSpec DeclContext 已正确（问题 1 已修复），核心工作是添加基类查找（问题 2）。
todos:
  - id: fix-qualified-lookup-bases
    content: 在 LookupQualifiedName 中添加基类遍历查找，改用 lookupInContext 避免意外穿透到父上下文
    status: pending
  - id: build-verify-433
    content: 编译验证 Task 4.3.3 修复
    status: pending
    dependencies:
      - fix-qualified-lookup-bases
---

## Task 4.3.3 Qualified Lookup 修复

### 问题 1：TypeSpec DeclContext 获取（已修复，无需操作）

当前代码第 308-309 行已正确使用 `static_cast<DeclContext *>(CXXRD)` 直接将 CXXRecordDecl 作为 DeclContext，而非 `CXXRD->getDeclContext()`。

### 问题 2：Qualified Lookup 不处理基类查找（需修复）

`LookupQualifiedName` 在 `Class::name` 形式的限定查找中，只在指定类自身的 DeclContext 中查找成员，不会遍历基类。C++ 语义要求：如果名字在当前类中未找到，应继续在基类中查找；如果找到的是函数，还需要收集基类中的重载候选。

### 核心功能

- 在 `LookupQualifiedName` 中添加基类遍历查找逻辑
- 当 DC 是 CXXRecordDecl 且类内未找到名字时，递归在基类中查找
- 如果类内找到函数声明，也要从基类中收集同名函数重载
- 使用 `SmallPtrSet` 防止菱形继承导致重复访问

## 技术方案

### 修改文件

- `src/Sema/Lookup.cpp` — 修改 `LookupQualifiedName` 函数

### 实现策略

在 `LookupQualifiedName` 的查找阶段（第 331-345 行），当 DC 是 CXXRecordDecl 时：

1. **先在类本身查找**：使用 `DC->lookupInContext(Name)`（仅当前上下文，不向父级），而非 `DC->lookup(Name)`（后者会沿 ParentCtx 向上查找命名空间等，不符合 qualified lookup 语义）
2. **未找到时遍历基类**：解析 `CXXRecordDecl::bases()` 中每个 `BaseSpecifier::getType()` 为 `CXXRecordDecl`，递归在其 DeclContext 中查找
3. **函数重载收集**：如果类内找到函数，也向基类收集同名函数作为重载候选
4. **防重复访问**：使用 `SmallPtrSet<const RecordType*, 8>` 追踪已访问的类型，避免菱形继承重复

### 关键实现细节

- 基类类型解析链路：`Base.getType()` → `QualType::getTypePtr()` → `isRecordType()` → `cast<RecordType>()` → `getDecl()` → `dyn_cast<CXXRecordDecl>(ASTNode*)`
- 当前 `DC->lookup(Name)` 会沿 ParentCtx 递归查找（见 DeclContext.cpp 第 35-44 行），对 CXXRecordDecl 而言其 ParentCtx 是命名空间，这会导致 qualified lookup 意外找到命名空间级的声明。应改为 `DC->lookupInContext(Name)` 仅在类内查找，基类遍历由新逻辑显式处理。