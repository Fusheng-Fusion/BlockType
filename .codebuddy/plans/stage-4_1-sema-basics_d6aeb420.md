---
name: stage-4.1-sema-basics
overview: 实现 Stage 4.1 Sema 基础设施（Task 4.1.1-4.1.4）：Sema 主类、符号表、Scope 完善、DeclContext 集成。完全参照 docs/plan/04-PHASE4-sema-basics.md 中 Stage 4.1 的设计。
todos:
  - id: create-sema-h
    content: 创建 include/blocktype/Sema/Sema.h（ExprResult/StmtResult/DeclResult + Sema 主类完整接口），按文档 E4.1.1.1 代码实现
    status: completed
  - id: create-sema-cpp
    content: 创建 src/Sema/Sema.cpp（构造/析构/Scope管理/DeclContext管理），按文档 E4.1.1.2 代码实现
    status: completed
    dependencies:
      - create-sema-h
  - id: create-symboltable-h
    content: 创建 include/blocktype/Sema/SymbolTable.h（6类分类存储 + addDecl/lookup），按文档 E4.1.2.1 代码实现
    status: completed
  - id: create-symboltable-cpp
    content: 创建 src/Sema/SymbolTable.cpp（所有 add/lookup 实现），按文档 E4.1.2.2 代码实现
    status: completed
    dependencies:
      - create-symboltable-h
  - id: modify-scope-h
    content: 修改 Scope.h 添加 using 指令支持（UsingDirectives + addUsingDirective + getUsingDirectives），按文档 E4.1.3.1
    status: completed
  - id: modify-cmake-and-diagnostics
    content: 更新 CMakeLists.txt 添加新源文件 + 添加 Sema 诊断 ID 到 DiagnosticIDs.def
    status: completed
    dependencies:
      - create-sema-cpp
      - create-symboltable-cpp
  - id: build-verify
    content: 编译验证，确保所有新文件与已有组件正确集成，修复编译错误
    status: completed
    dependencies:
      - modify-cmake-and-diagnostics
---

## 产品概述

为 BlockType 编译器实现 Phase 4 Stage 4.1 语义分析基础设施。本阶段是 Phase 4 的**奠基工程**，不仅实现 Stage 4.1 自身功能（Sema 主类 + 符号表 + Scope 完善 + DeclContext 集成），还**创建 Phase 4 所有后续阶段（4.2-4.5）需要的全部 stub 头文件、方法签名和诊断 ID**，彻底避免 Phase 3 中后期随机编码的乱象。

## 核心功能

- **Task 4.1.1 Sema 主类**: `Sema.h`（ExprResult/StmtResult/DeclResult + Sema 主类含全阶段方法签名）+ `Sema.cpp`（核心生命周期和作用域管理实现 + 其余方法 stub）
- **Task 4.1.2 符号表**: `SymbolTable.h`（6 类分类存储）+ `SymbolTable.cpp`（完整实现含函数重载支持和重定义检查）
- **Task 4.1.3 Scope 完善**: 添加 using 指令支持
- **Task 4.1.4 DeclContext 集成**: 确保 Sema 正确使用已有 DeclContext
- **前置 Stub 创建**: 7 个 stub 头文件（Lookup.h, Conversion.h, Overload.h, TypeDeduction.h, TypeCheck.h, AccessControl.h, ConstantExpr.h）+ DiagnosticSemaKinds.def
- **辅助变更**: 更新 CMakeLists.txt、引入诊断 ID

## 技术栈

- 语言: C++17
- 依赖: LLVM ADT（StringMap, SmallVector, ArrayRef, DenseMap, APSInt, APFloat, raw_ostream, SmallPtrSet）
- 构建系统: CMake
- 命名空间: `blocktype`
- 头文件前缀: `blocktype/...`

## 实现方案

严格参照 `docs/plan/04-PHASE4-sema-basics.md` 中**全部 5 个 Stage** 的设计，在已有的 Scope（198行）和 DeclContext（181行）基础上构建 Sema 框架。Stage 4.1 创建的文件包含后续阶段所需的全部类型定义和方法签名。

### 关键设计决策

1. **一次性创建所有 stub 头文件**：不仅是 4.3/4.4 的 Lookup/Overload/Conversion，还包括 4.2 的 TypeDeduction 和 4.5 的 TypeCheck/AccessControl/ConstantExpr
2. **Sema.h 包含所有后续阶段的 include 和方法签名**：不做任何删减
3. **DiagnosticSemaKinds.def 一次到位**：28+ 个诊断 ID 全部预定义
4. **Sema.cpp 中未实现方法提供 stub**：返回 Invalid/空/nullptr，确保编译通过
5. **SymbolTable 完整实现**：含函数重载和重定义检查
6. **Scope 通过 new/delete 管理**（已在 Scope.cpp 中验证）
7. **CurContext 指针链**：通过 PushDeclContext/PopDeclContext 维护

### 性能考量

- SymbolTable 的 StringMap 查找 O(1) 平均
- Scope 的 StringMap 查找 O(1) 平均
- 函数重载使用 SmallVector<NamedDecl*, 4>，少量重载时不堆分配

## 架构设计

```
Phase 4 全阶段架构（Stage 4.1 创建全部骨架）
═══════════════════════════════════════════

Sema（协调器）
├── SymbolTable（全局符号存储）[4.1 完整实现]
├── CurrentScope → Scope 链 [4.1 完整实现]
├── CurContext → DeclContext 链 [4.1 完整实现]
├── [4.3 预声明] LookupUnqualifiedName/LookupQualifiedName/LookupADL
├── [4.4 预声明] ResolveOverload/AddOverloadCandidate
└── [4.5 预声明] Diag 辅助方法

Stub 头文件（4.1 创建，后续阶段实现 .cpp）
├── Lookup.h → E4.3.1.1（LookupNameKind + LookupResult + NestedNameSpecifier）
├── Conversion.h → E4.4.2.1（ConversionRank + SCS + ICS + ConversionChecker）
├── Overload.h → E4.4.1.1（OverloadCandidate + OverloadCandidateSet）
├── TypeDeduction.h → E4.2.2.1（auto/decltype 完整接口）
├── TypeCheck.h → E4.5.1.1（赋值/初始化/调用检查）
├── AccessControl.h → E4.5.2.1（成员/基类/友元访问）
├── ConstantExpr.h → E4.5.4.1（EvalResult + ConstantExprEvaluator）
└── DiagnosticSemaKinds.def → E4.5.3.1（28+ 诊断 ID）
```

## 目录结构

```
include/blocktype/Sema/
├── Sema.h            # Sema 主类（含全阶段方法签名）
├── SymbolTable.h     # 符号表（6 类分类存储）
├── Scope.h           # [MODIFY] 添加 using 指令支持
├── Lookup.h          # [NEW stub] 名字查找类型定义
├── Conversion.h      # [NEW stub] 隐式转换类型定义
├── Overload.h        # [NEW stub] 重载决议类型定义
├── TypeDeduction.h   # [NEW stub] 类型推导接口
├── TypeCheck.h       # [NEW stub] 类型检查接口
├── AccessControl.h   # [NEW stub] 访问控制接口
└── ConstantExpr.h    # [NEW stub] 常量表达式求值接口

src/Sema/
├── Sema.cpp          # [NEW] 核心实现 + stub
├── SymbolTable.cpp   # [NEW] 完整实现
├── Scope.cpp         # [已有]
└── CMakeLists.txt    # [MODIFY] 添加新源文件

include/blocktype/Basic/
└── DiagnosticSemaKinds.def  # [NEW] Phase 4 全阶段诊断 ID
```

## 实现注意事项

- **Phase 3 教训的彻底回应**：所有后续阶段需要的方法名、参数类型、返回类型、诊断 ID 必须在 Stage 4.1 预先确定
- **Sema.h 保留所有原始 include**（Lookup.h, Overload.h 等），对应 stub 在 Stage 4.1 一并创建
- **所有 stub 头文件与原文档代码完全一致**，不做任何删减
- **Sema.cpp 每个 stub 方法有 TODO 注释**，标明应在哪个 Stage 实现
- **SymbolTable 构造函数需要 ASTContext&**，暂存引用供未来扩展
- **Scope 的 using 指令**：需在 Scope.h 中通过已有 Decl.h include 解决 NamespaceDecl 可见性
- **NestedNameSpecifier 使用 union**：需注意 C++ 限制（所有成员必须是平凡类型）
- **ConstantExpr.h** 使用 llvm::APFloat 和 llvm::APSInt，需 include 对应头文件
- **DiagnosticSemaKinds.def** 使用与 DiagnosticIDs.def 相同的 DIAG 宏格式

## SubAgent

- **code-explorer**: 用于编译验证时快速定位编译错误和相关依赖文件，确保新代码与已有组件正确集成