---
name: task-4.2.2-type-deduction
overview: 修复 Task 4.2.2 类型推导中的 4 个问题：给 Expr 添加 ValueKind 枚举和值类别查询方法，修复 deduceFromInitList 类型比较，修复 deduceAutoForwardingRefType 值类别区分，修复 deduceDecltypeType 值类别保留
todos:
  - id: add-valuekind
    content: 在 Expr.h 中添加 ValueKind 枚举，Expr 基类添加 VK 字段和虚方法，关键子类覆写 getValueKind()
    status: completed
  - id: fix-forwarding-ref
    content: 修复 deduceAutoForwardingRefType：根据 isLValue() 区分 T& vs T&&
    status: completed
    dependencies:
      - add-valuekind
  - id: fix-decltype
    content: 修复 deduceDecltypeType：根据 getValueKind() 保留值类别引用
    status: completed
    dependencies:
      - add-valuekind
  - id: fix-initlist
    content: 修复 deduceFromInitList：改用 getCanonicalType() 比较类型
    status: completed
  - id: build-verify
    content: 编译验证所有修复
    status: completed
    dependencies:
      - fix-forwarding-ref
      - fix-decltype
      - fix-initlist
---

## Product Overview

修复 Task 4.2.2 类型推导中发现的 4 个问题，核心是为 Expr 基类添加值类别（ValueKind）支持，并修复 3 个类型推导函数。

## Core Features

1. **添加 Expr::ValueKind 枚举** — 在 Expr 基类中添加 `ValueKind { LValue, XValue, PRValue }` 枚举、`VK` 字段（默认 PRValue 向后兼容）、`getValueKind()`/`setValueKind()`/`isLValue()`/`isXValue()`/`isPRValue()` 方法，并在关键子类中覆写默认值（DeclRefExpr→LValue, StringLiteral→LValue, UnaryOperator(Deref)→LValue 等）

2. **修复 deduceAutoForwardingRefType** — 根据 Init 表达式的值类别区分：lvalue→T&（左值引用），rvalue→T&&（右值引用），而非始终推导为右值引用

3. **修复 deduceDecltypeType** — 根据表达式的值类别保留引用：lvalue→T&，xvalue→T&&，prvalue→T

4. **修复 deduceFromInitList** — 将 `T.getTypePtr() != FirstType.getTypePtr()` 指针比较改为 `getCanonicalType()` 比较

注：deduceAutoRefType 未检查值类别合法性属于 Stage 4.5 类型检查范畴，本次不修复。

## Tech Stack

沿用项目现有 C++ 技术栈：LLVM 基础库 (BumpPtrAllocator, StringRef, DenseMap 等)，无新依赖。

## Implementation Approach

### 1. Expr::ValueKind 设计

参照 Clang 的 `clang::Expr::ValueKind`，在 Expr 基类中添加值类别支持：

- 枚举 `ValueKind { LValue, XValue, PRValue }` 放在 Expr.h 顶部（与其他枚举如 BinaryOpKind 同级）
- Expr 基类新增 `ValueKind VK = ValueKind::PRValue` 字段，默认 PRValue 保证向后兼容（所有现有构造函数无需修改）
- 虚方法 `getValueKind()` 返回 VK，子类可覆写
- 便捷方法 `isLValue()`/`isXValue()`/`isPRValue()`
- `setValueKind()` 供 Sema 后续阶段使用

### 2. 子类覆写策略

仅在值类别**静态确定**的子类中覆写 `getValueKind()`：

- **DeclRefExpr** → LValue（变量/函数名是左值）
- **StringLiteral** → LValue（字符串字面量是左值，`"hello"` 有地址）
- **MemberExpr** → LValue（成员访问的结果通常是左值）
- **ArraySubscriptExpr** → LValue（下标运算符的结果是左值）
- **UnaryOperator(Deref)** → LValue（解引用是左值）

其余子类保持默认 PRValue 即可（字面量、二元运算、函数调用等都是纯右值）。

### 3. TypeDeduction 修复

- **deduceAutoForwardingRefType**: 调用 `Init->isLValue()` 判断，lvalue→`getLValueReferenceType`，rvalue→`getRValueReferenceType`
- **deduceDecltypeType**: 根据 `E->getValueKind()` 分三路：LValue→`T&`，XValue→`T&&`，PRValue→`T`
- **deduceFromInitList**: 将 `T.getTypePtr() != FirstType.getTypePtr()` 改为 `T.getCanonicalType() != FirstType.getCanonicalType()`

## Implementation Notes

- ValueKind 字段使用默认初始化 PRValue，**不修改任何现有 Expr 子类构造函数签名**，避免大范围重构
- 子类覆写通过 `return ValueKind::LValue;` 实现，无需存储额外字段
- `QualType::getCanonicalType()` 已有基础实现（对 BuiltinType 等直接返回 self），对当前测试用例足够；完整 canonical type 系统需要后续迭代
- deduceAutoRefType 的值类别合法性检查（`auto& x = rvalue` 应报错）属于 Stage 4.5 诊断范畴，本次仅添加 TODO 注释

## Directory Structure

```
include/blocktype/AST/
├── Expr.h                     # [MODIFY] 添加 ValueKind 枚举、Expr 基类 VK 字段/getter/setter，
└──                             #         子类覆写 getValueKind() (DeclRefExpr, StringLiteral, MemberExpr, ArraySubscriptExpr, UnaryOperator)

src/Sema/
└── TypeDeduction.cpp           # [MODIFY] 修复 deduceAutoForwardingRefType（值类别区分）、

#         deduceDecltypeType（值类别保留）、deduceFromInitList（canonical 比较）

````