---
name: task-4.2.1-type-factory-completion
overview: 修复 ASTContext 类型工厂的 3 个问题：getTypeDeclType 绕过缓存、getMemberFunctionType CVR 语义错误、补充便捷类型方法
todos:
  - id: fix-typedcltype-cache
    content: "修复 getTypeDeclType: RecordDecl/EnumDecl 分支改用 getRecordType/getEnumType 缓存路径"
    status: completed
  - id: fix-functiontype-cv
    content: 给 FunctionType 添加 MethodQualifiers 字段 (IsConst/IsVolatile)，扩展构造函数和 getFunctionType 签名
    status: completed
  - id: fix-memberfn-cvr
    content: "修复 getMemberFunctionType: CV 存入 FunctionType 而非 MemberPointer Qualifier"
    status: completed
    dependencies:
      - fix-functiontype-cv
  - id: add-builtin-conv
    content: 补充 Char/SChar/UChar/Short/UShort/UInt/ULong/LongLong/ULongLong/LongDouble 便捷工厂方法
    status: completed
  - id: build-verify
    content: 编译验证所有修复
    status: completed
    dependencies:
      - fix-memberfn-cvr
      - add-builtin-conv
---

## Product Overview

修复 Task 4.2.1 类型工厂补全中发现的问题，确保类型系统的正确性和完整性。

## Core Features

1. **修复 getTypeDeclType 绕过缓存** -- `getTypeDeclType()` 中 RecordDecl/EnumDecl 分支直接 new 对象，不走 `getRecordType()`/`getEnumType()` 的 DenseMap 缓存路径，破坏类型唯一性
2. **修复 getMemberFunctionType CVR 语义** -- const/volatile 应存储在 FunctionType 的 MethodQualifiers 字段中（符合 C++ 语义：`R (C::*)(Args) const` 的 const 属于函数类型），而非附加到 MemberPointerType 的 Qualifier 上
3. **补充 BuiltinType 便捷方法** -- 为 BuiltinTypes.def 中已定义但缺少便捷方法的 Char/SChar/UChar/Short/UShort/UInt/ULong/LongLong/ULongLong/LongDouble 等类型添加工厂方法

## Tech Stack

沿用项目现有 C++ 技术栈：LLVM 基础库 (DenseMap, BumpPtrAllocator, StringRef 等)

## Implementation Approach

### 问题 1: getTypeDeclType 绕过缓存

将 `getTypeDeclType()` 中 RecordDecl/EnumDecl 分支改为调用已有的 `getRecordType()`/`getEnumType()`，直接复用 DenseMap 缓存路径。

### 问题 2: FunctionType 添加 MethodQualifiers

参照 Clang 的 `FunctionProtoType` 设计，给 `FunctionType` 添加：

- `bool IsConst` / `bool IsVolatile` 字段（用于成员函数的 cv-qualifier）
- 构造函数扩展参数，保持默认值向后兼容
- getter 方法：`isConst()`, `isVolatile()`
- `getMemberFunctionType` 改为将 CV 传入 FunctionType，而非附加到 MemberPointerType 的 Qualifier 上
- `getFunctionType` 同步扩展以支持 MethodQualifiers 参数

### 问题 3: 补充 BuiltinType 便捷方法

参照现有 `getVoidType()`/`getIntType()` 模式，为 BuiltinTypes.def 中所有已定义类型添加便捷工厂方法。

## Implementation Notes

- FunctionType 构造函数扩展必须使用默认参数值，避免破坏所有现有调用点
- `getFunctionType` 也需扩展 MethodQualifiers 参数（默认 false），确保 `getMemberFunctionType` 能传递 CV 到 FunctionType
- `getMemberFunctionType` 返回值不再附加 Qualifier，直接返回裸 MemberPointerType

## Directory Structure

```
include/blocktype/AST/
├── Type.h                    # [MODIFY] FunctionType 添加 IsConst/IsVolatile 字段和 getter
└── ASTContext.h              # [MODIFY] 添加 9 个便捷方法声明 + getFunctionType 扩展签名

src/AST/
└── ASTContext.cpp            # [MODIFY] getTypeDeclType 使用缓存路径 + getMemberFunctionType 修复 + 便捷方法实现
```