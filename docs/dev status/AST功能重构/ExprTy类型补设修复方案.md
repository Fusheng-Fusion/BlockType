# ExprTy 类型补设修复方案

> **状态**: 方案设计  
> **前置依赖**: 阶段 3 已完成（Parser 零 `Context.create`，662/662 测试通过）  
> **当前测试基线**: 662/662 全部通过  
> **目标**: 所有表达式节点在 `ActOn*` 工厂方法中创建时即设置正确 `ExprTy`，**最终删除 `ProcessAST`**  
> **风险等级**: 中高（修改 25+ 个表达式工厂方法，需确保类型推导链完整）
> **最终状态**: ProcessAST → 完全删除，driver.cpp 编译流程简化为 Parse → Sema（内嵌于 ActOn*）→ CodeGen

---

## 一、问题描述

### 1.1 现状

阶段 3 完成了 Parser → Sema 的全部委托迁移（`Context.create` 从 172 → 0），但采用了**防御性迁移策略**：在子表达式类型可能尚未就绪时跳过类型设置，留给 `ProcessAST` 后处理。

然而 `ProcessAST::visitExpr` 只处理了 `CXXNewExpr` 和 `CXXDeleteExpr` 两种，**其余 25 种表达式节点的类型永远不会被设置**。

### 1.2 影响

- **CodeGen 依赖 `getType()`**：IR 生成需要表达式类型来选择正确的指令（整数 vs 浮点、有符号 vs 无符号、指针运算等）
- **类型推导链断裂**：父表达式的类型依赖子表达式类型，如果子表达式 `ExprTy` 为空，父表达式也无法推导
- **测试仍在通过**：因为 CodeGen 对 `getType().isNull()` 有防御处理，但这种防御掩盖了架构缺陷

### 1.3 根因分类

| 类型 | 原因 | 示例 |
|------|------|------|
| **A. 信息已有但未使用** | 方法内部已有足够信息推导类型，但没调用 `setType()` | `ActOnConditionalExpr` 计算了 `ResultType` 但没设置 |
| **B. 参数已有但未传递** | 调用方已传入类型信息，工厂方法未使用 | `ActOnCastExpr` 接收 `TargetType` 但没设给节点 |
| **C. 需要新增逻辑** | 方法内无类型推导逻辑，需要新增 | `ActOnDeclRefExpr` 需要 `D->getType()` |
| **D. 防御性跳过** | 有推导逻辑但被 `getType().isNull()` 守卫跳过 | `ActOnBinaryOperator` 在子类型为空时跳过 |
| **E. 类型推导复杂** | 需要更复杂的推导或无法在创建时确定 | `ActOnInitListExpr` 需要聚合类型推导 |

---

## 二、修复范围

### 2.1 全量审计清单

#### Tier 1 — 高优先级（核心表达式，CodeGen 直接依赖）

| # | 方法 | 行号 | 应设置类型 | 根因 | 修复策略 |
|---|------|------|-----------|------|---------|
| 1 | `ActOnDeclRefExpr` | 703 | `D->getType()` | C | 直接取 Decl 类型 |
| 2 | `ActOnCallExpr` | 844 | `FD->getReturnType()` | C | 从 FunctionDecl 的 FunctionType 中提取返回类型 |
| 3 | `ActOnMemberExpr` | 923 | `MemberDecl->getType()` | C+D | 移除防御跳过，在查找到成员后设类型 |
| 4 | `ActOnMemberExprDirect` | 744 | `MemberDecl->getType()` | C | 直接取成员类型 |
| 5 | `ActOnCastExpr` | 1040 | `TargetType` | B | 直接设置传入的目标类型 |
| 6 | `ActOnArraySubscriptExpr` | 1056 | 元素类型（解引用 BaseType） | C | 从指针/数组类型推导元素类型 |
| 7 | `ActOnConditionalExpr` | 1087 | `TC.getCommonType()` | A | 将计算结果 `ResultType` 赋给 `CO->setType()` |

#### Tier 2 — 中优先级（C++ 特有表达式，影响 C++ 代码生成）

| # | 方法 | 行号 | 应设置类型 | 根因 | 修复策略 |
|---|------|------|-----------|------|---------|
| 8 | `ActOnBinaryOperator` | 986 | `TC.getBinaryOperatorResultType()` | D | 移除防御跳过，保证子表达式类型非空后计算 |
| 9 | `ActOnUnaryOperator` | 1014 | `TC.getUnaryOperatorResultType()` | D | 移除防御跳过，保证子表达式类型非空后计算 |
| 10 | `ActOnCXXNamedCastExpr` | 780 | Cast 目标类型（需扩展签名） | C | 需要传入目标类型参数 |
| 11 | `ActOnCXXNamedCastExprWithType` | 791 | `CastType` | B | 将传入的 `CastType` 设给节点 |
| 12 | `ActOnUnaryExprOrTypeTraitExpr(kind, QualType)` | 708 | `Context.getSizeType()` | C | sizeof/alignof 返回 `size_t` |
| 13 | `ActOnUnaryExprOrTypeTraitExpr(kind, Expr*)` | 715 | `Context.getSizeType()` | C | 同上 |
| 14 | `ActOnCXXThisExpr` | 770 | `CurContext` 的指针类型 | C | 从当前类的 CXXRecordDecl 构造指针类型 |
| 15 | `ActOnCXXConstructExpr` | 750 | 构造的类类型（需扩展签名） | C | 需要传入目标类型 |
| 16 | `ActOnCXXNewExprFactory` | 756 | `Context.getPointerType(Type)` | C | `new T` 结果类型是 `T*` |
| 17 | `ActOnCXXDeleteExprFactory` | 762 | `Context.getVoidType()` | C | delete 结果类型是 void |
| 18 | `ActOnCXXThrowExpr` | 775 | `Context.getVoidType()` | C | throw 结果类型是 void |
| 19 | `ActOnPostIncrement` | — | 同操作数类型 | C | 后缀 ++ 结果类型与操作数相同 |
| 20 | `ActOnPostDecrement` | — | 同操作数类型 | C | 后缀 -- 结果类型与操作数相同 |

#### Tier 3 — 低优先级（高级特性，当前测试可能不涉及）

| # | 方法 | 行号 | 应设置类型 | 根因 | 修复策略 |
|---|------|------|-----------|------|---------|
| 21 | `ActOnInitListExpr` | 722 | 聚合类型推导 | E | 需要从上下文推断（变量声明类型或函数参数类型） |
| 22 | `ActOnDesignatedInitExpr` | 729 | 同上 | E | 依赖初始化目标类型 |
| 23 | `ActOnTemplateSpecializationExpr` | 737 | 模板实例类型 | C | 需要模板实例化结果 |
| 24 | `ActOnLambdaExpr` | 812 | closure 类型 | E | 需要创建唯一的 closure class 类型 |
| 25 | `ActOnCXXFoldExpr` | 827 | 折叠表达式类型 | E | 依赖模式类型推导 |
| 26 | `ActOnRequiresExpr` | 835 | `Context.getBoolType()` | C | requires 表达式总是 bool |
| 27 | `ActOnPackIndexingExpr` | 801 | 包元素类型 | E | 依赖包展开 |
| 28 | `ActOnReflexprExpr` | 807 | 类型信息 | E | 反射类型，需要 type_info 类型 |
| 29 | `ActOnCXXForRangeStmt` | 1251 | （Stmt，非 Expr） | — | 此为 Stmt 方法，不影响 ExprTy |

---

## 三、分阶段修复计划

### 阶段 4A：Tier 1 高优先级修复（7 处）

> 预计修改文件：`src/Sema/Sema.cpp`  
> 预计工作量：1-2 小时  
> 风险：中

#### 4A-1. `ActOnDeclRefExpr` — 设置 Decl 类型

```cpp
// 当前代码 (行 703-706):
ExprResult Sema::ActOnDeclRefExpr(SourceLocation Loc, ValueDecl *D) {
  auto *DRE = Context.create<DeclRefExpr>(Loc, D);
  return ExprResult(DRE);
}

// 修复后:
ExprResult Sema::ActOnDeclRefExpr(SourceLocation Loc, ValueDecl *D) {
  auto *DRE = Context.create<DeclRefExpr>(Loc, D);
  if (D && !D->getType().isNull())
    DRE->setType(D->getType());
  return ExprResult(DRE);
}
```

**原理**：`ValueDecl` 基类有 `getType()` 方法，返回声明的类型。`DeclRefExpr` 引用一个声明，其表达式类型就是声明的类型。

#### 4A-2. `ActOnCallExpr` — 设置函数返回类型

```cpp
// 在创建 CallExpr 后设置类型（行 919 之后）:
auto *CE = Context.create<CallExpr>(LParenLoc, Fn, Args);
// 从 FunctionDecl 提取返回类型
if (FD) {
  QualType FnType = FD->getType();
  if (auto *FT = llvm::dyn_cast<FunctionType>(FnType.getTypePtr())) {
    CE->setType(QualType(FT->getReturnType(), Qualifier::None));
  }
}
return ExprResult(CE);
```

**注意**：此方法有多个 `Context.create<CallExpr>` 出口（行 857/889/897/910/919），每个出口都需设置类型。有 `FD` 的出口可从 `FD` 获取返回类型；无 `FD` 的出口（未解析函数）保持 `ExprTy` 为空，留给后续处理。

**风险**：`FunctionType::getReturnType()` 返回 `const Type*`，需包装为 `QualType`。需验证 FunctionType 的 ReturnType 是否在解析阶段已正确设置。

#### 4A-3. `ActOnMemberExpr` — 移除防御跳过 + 设置成员类型

```cpp
// 行 982 之后:
auto *ME = Context.create<MemberExpr>(MemberLoc, Base, MemberDecl, IsArrow);
if (MemberDecl && !MemberDecl->getType().isNull())
  ME->setType(MemberDecl->getType());
return ExprResult(ME);
```

**注意**：行 932 的防御跳过逻辑（`BaseType.isNull()` 时创建空 MemberExpr）暂时保留，因为 Base 的类型可能在某些解析路径下确实为空（嵌套模板等）。但成功查找到 MemberDecl 的路径应设类型。

#### 4A-4. `ActOnMemberExprDirect` — 设置成员类型

```cpp
// 行 744-748:
ExprResult Sema::ActOnMemberExprDirect(SourceLocation OpLoc, Expr *Base,
                                       ValueDecl *MemberDecl, bool IsArrow) {
  auto *ME = Context.create<MemberExpr>(OpLoc, Base, MemberDecl, IsArrow);
  if (MemberDecl && !MemberDecl->getType().isNull())
    ME->setType(MemberDecl->getType());
  return ExprResult(ME);
}
```

#### 4A-5. `ActOnCastExpr` — 设置目标类型

```cpp
// 行 1052:
auto *CE = Context.create<CStyleCastExpr>(LParenLoc, E);
CE->setType(TargetType);  // ← 新增：TargetType 已传入但未使用
return ExprResult(CE);
```

**原理**：C 风格 cast 表达式的类型就是目标类型，`TargetType` 参数已经传入。

#### 4A-6. `ActOnArraySubscriptExpr` — 设置元素类型

```cpp
// 行 1083 之后:
auto *ASE = Context.create<ArraySubscriptExpr>(LLoc, Base, Indices);
if (!BaseType.isNull()) {
  const Type *BaseTy = BaseType.getTypePtr();
  if (auto *PT = llvm::dyn_cast<PointerType>(BaseTy)) {
    ASE->setType(PT->getPointeeType());
  } else if (auto *AT = llvm::dyn_cast<ArrayType>(BaseTy)) {
    ASE->setType(QualType(AT->getElementType(), Qualifier::None));
  }
}
return ExprResult(ASE);
```

#### 4A-7. `ActOnConditionalExpr` — 修复 bug + 设置计算结果

```cpp
// 当前代码 (行 1098-1107) 有 bug:
//   计算了 ResultType 但从未调用 CO->setType(ResultType)
// 修复:
if (!Then->getType().isNull() && !Else->getType().isNull()) {
  QualType ResultType = TC.getCommonType(Then->getType(), Else->getType());
  if (ResultType.isNull()) {
    Diags.report(ColonLoc, DiagID::err_type_mismatch);
    return ExprResult::getInvalid();
  }
  auto *CO = Context.create<ConditionalOperator>(QuestionLoc, Cond, Then, Else);
  CO->setType(ResultType);  // ← 新增
  return ExprResult(CO);
}

auto *CO = Context.create<ConditionalOperator>(QuestionLoc, Cond, Then, Else);
return ExprResult(CO);
```

### 阶段 4B：Tier 2 中优先级修复（13 处）

> 预计修改文件：`src/Sema/Sema.cpp`，`include/blocktype/Sema/Sema.h`  
> 预计工作量：2-3 小时  
> 风险：中

#### 4B-1. `ActOnBinaryOperator` — 移除防御跳过

**前提**：阶段 4A 完成后，`DeclRefExpr` 等子表达式已有类型，防御跳过可移除。

```cpp
// 移除行 996-999 的防御跳过:
//   if (LHSType.isNull() || RHSType.isNull()) {
//     auto *BO = Context.create<BinaryOperator>(OpLoc, LHS, RHS, Op);
//     return ExprResult(BO);
//   }
// 改为直接计算：
if (LHSType.isNull() || RHSType.isNull()) {
  Diags.report(OpLoc, DiagID::err_type_mismatch);
  return ExprResult::getInvalid();
}
```

**风险**：可能暴露之前被防御逻辑掩盖的类型缺失 bug。需逐步推进，每改一处跑全量测试。

#### 4B-2. `ActOnUnaryOperator` — 移除防御跳过

同 4B-1 策略，移除行 1022-1025 的防御跳过。

#### 4B-3~4. `ActOnUnaryExprOrTypeTraitExpr` — 设置 `size_t`

```cpp
ExprResult Sema::ActOnUnaryExprOrTypeTraitExpr(SourceLocation Loc,
                                               UnaryExprOrTypeTrait Kind,
                                               QualType T) {
  auto *E = Context.create<UnaryExprOrTypeTraitExpr>(Loc, Kind, T);
  E->setType(Context.getSizeType());  // sizeof/alignof 返回 size_t
  return ExprResult(E);
}

ExprResult Sema::ActOnUnaryExprOrTypeTraitExpr(SourceLocation Loc,
                                               UnaryExprOrTypeTrait Kind,
                                               Expr *Arg) {
  auto *E = Context.create<UnaryExprOrTypeTraitExpr>(Loc, Kind, Arg);
  E->setType(Context.getSizeType());  // sizeof/alignof 返回 size_t
  return ExprResult(E);
}
```

**注意**：需确认 `ASTContext::getSizeType()` 已存在。如果不存在，需要添加：
```cpp
QualType ASTContext::getSizeType() const {
  return QualType(BuiltinTypes[BuiltinKind::ULong], Qualifier::None);
}
```

#### 4B-5~6. `ActOnCXXNamedCastExpr` / `ActOnCXXNamedCastExprWithType`

```cpp
// ActOnCXXNamedCastExprWithType — 使用传入的 CastType:
ExprResult Sema::ActOnCXXNamedCastExprWithType(SourceLocation CastLoc,
                                               Expr *SubExpr,
                                               QualType CastType,
                                               llvm::StringRef CastKind) {
  if (CastKind == "dynamic_cast") {
    auto *E = Context.create<CXXDynamicCastExpr>(CastLoc, SubExpr, CastType);
    E->setType(CastType);
    return ExprResult(E);
  }
  // 其他 cast 委托给 ActOnCXXNamedCastExpr，需传入 CastType
  // 需要修改 ActOnCXXNamedCastExpr 签名添加 CastType 参数
  ...
}
```

**注意**：`ActOnCXXNamedCastExpr` 当前签名没有 `CastType` 参数，需要扩展签名。

#### 4B-7. `ActOnCXXThisExpr` — 设置当前类指针类型

```cpp
ExprResult Sema::ActOnCXXThisExpr(SourceLocation Loc) {
  auto *TE = Context.create<CXXThisExpr>(Loc);
  if (auto *RD = llvm::dyn_cast_or_null<CXXRecordDecl>(CurContext)) {
    auto *PtrTy = Context.getPointerType(
        QualType(RD->getTypeForDecl(), Qualifier::None));
    TE->setType(QualType(PtrTy, Qualifier::None));
  }
  return ExprResult(TE);
}
```

**注意**：需确认 `CurContext` 在 `Sema` 中维护，且指向当前正在处理的 `CXXRecordDecl`。如果 `CurContext` 不存在，需要从 `CurrentScope` 中查找。

#### 4B-8. `ActOnCXXNewExprFactory` — 设置 `T*`

```cpp
ExprResult Sema::ActOnCXXNewExprFactory(SourceLocation NewLoc, Expr *ArraySize,
                                         Expr *Initializer, QualType Type) {
  auto *NE = Context.create<CXXNewExpr>(NewLoc, ArraySize, Initializer, Type);
  if (!Type.isNull()) {
    auto *PtrTy = Context.getPointerType(Type.getTypePtr());
    NE->setType(QualType(PtrTy, Qualifier::None));
  }
  return ExprResult(NE);
}
```

#### 4B-9. `ActOnCXXDeleteExprFactory` — 设置 `void`

```cpp
ExprResult Sema::ActOnCXXDeleteExprFactory(SourceLocation DeleteLoc,
                                           Expr *Argument, bool IsArrayDelete,
                                           QualType AllocatedType) {
  auto *DE = Context.create<CXXDeleteExpr>(DeleteLoc, Argument, IsArrayDelete,
                                            AllocatedType);
  auto *VoidType = Context.getBuiltinType(BuiltinKind::Void);
  DE->setType(QualType(VoidType, Qualifier::None));
  return ExprResult(DE);
}
```

#### 4B-10. `ActOnCXXThrowExpr` — 设置 `void`

```cpp
ExprResult Sema::ActOnCXXThrowExpr(SourceLocation Loc, Expr *Operand) {
  auto *TE = Context.create<CXXThrowExpr>(Loc, Operand);
  auto *VoidType = Context.getBuiltinType(BuiltinKind::Void);
  TE->setType(QualType(VoidType, Qualifier::None));
  return ExprResult(TE);
}
```

#### 4B-11. `ActOnCXXConstructExpr` — 需扩展签名

```cpp
// 需要新增 ConstructedType 参数：
ExprResult Sema::ActOnCXXConstructExpr(SourceLocation Loc,
                                       QualType ConstructedType,
                                       llvm::ArrayRef<Expr *> Args) {
  auto *CE = Context.create<CXXConstructExpr>(Loc, Args);
  CE->setType(ConstructedType);
  return ExprResult(CE);
}
```

**影响范围**：需同步修改 `Sema.h` 声明和 `ParseExprCXX.cpp` 中的调用点。

### 阶段 4C：Tier 3 低优先级修复（8 处）

> 预计修改文件：`src/Sema/Sema.cpp`，`include/blocktype/Sema/Sema.h`  
> 预计工作量：3-4 小时  
> 风险：低（当前测试可能不涉及这些表达式类型）

#### 4C-1. `ActOnRequiresExpr` — 设置 `bool`

```cpp
ExprResult Sema::ActOnRequiresExpr(...) {
  auto *RE = Context.create<RequiresExpr>(...);
  RE->setType(Context.getBoolType());
  return ExprResult(RE);
}
```

#### 4C-2~8. 初始化列表、Lambda、Fold 表达式等

这些需要更复杂的类型推导机制：

- **InitListExpr**：需从变量声明或函数参数的预期类型推导
- **LambdaExpr**：需为每个 lambda 创建唯一的匿名 class 类型
- **CXXFoldExpr**：需推导折叠结果类型
- **PackIndexingExpr**：需展开包后推导
- **ReflexprExpr**：需定义反射类型系统
- **DesignatedInitExpr**：依赖初始化目标类型
- **TemplateSpecializationExpr**：需模板实例化结果

这些建议标记为 `TODO`，在对应功能开发时一并实现。

### 阶段 4D：清除 ProcessAST（最终目标）

> 预计修改文件：`src/Sema/Sema.cpp`，`include/blocktype/Sema/Sema.h`，`tools/driver.cpp`  
> 预计工作量：0.5 小时  
> 风险：低（纯删除操作）

**原理**：`ProcessAST` 从一开始就是临时补丁。阶段 1 将类型设置从 CodeGen 移到 Sema::ProcessAST，阶段 3 将节点创建从 Parser 移到 Sema::ActOn*。现在阶段 4A/4B 让所有 ActOn* 在创建时即设类型，ProcessAST **彻底失去存在意义**。

#### 4D-1. 删除 `Sema::ProcessAST()` 方法及其内部 `ASTVisitor` 类

```cpp
// 删除 src/Sema/Sema.cpp 行 1502-1629 的全部代码：
// - anonymous namespace 中的 ASTVisitor 类
// - Sema::ProcessAST() 方法实现
```

#### 4D-2. 删除 `Sema::ActOnCXXNewExpr(CXXNewExpr*)` 和 `Sema::ActOnCXXDeleteExpr(CXXDeleteExpr*)` 后处理方法

这两个方法是 ProcessAST 的附属品——它们接收**已创建**的节点来补设类型。现在 `ActOnCXXNewExprFactory` 和 `ActOnCXXDeleteExprFactory` 在创建时已设类型，后处理方法变为死代码。

```cpp
// 删除 Sema.h 中的声明：
//   ExprResult ActOnCXXNewExpr(CXXNewExpr *E);
//   ExprResult ActOnCXXDeleteExpr(CXXDeleteExpr *E);
//   void ProcessAST(TranslationUnitDecl *TU);

// 删除 Sema.cpp 中的实现（行 1459-1500, 1625-1629）
```

#### 4D-3. 从 `driver.cpp` 移除 ProcessAST 调用

```cpp
// 删除 driver.cpp 行 218-219:
//    // 8. 语义分析后处理（仅处理 Parser 未委托 Sema 创建的节点）
//    S.ProcessAST(TU);

// 编译流程简化为：
//    Parser P(PP, Context, S);          // 6. 创建解析器
//    TranslationUnitDecl *TU = P.parseTranslationUnit();  // 解析（Sema 内嵌处理）
//    // Sema 的工作在解析过程中通过 ActOn* 调用完成，无需后处理
```

#### 4D-4. 简化 `Sema::ActOnExpr()` 通用类型补设

```cpp
// 当前 ActOnExpr (行 620-657) 包含对 BinaryOperator/UnaryOperator 的通用类型补设
// 这是 ProcessAST 模式的残留——在节点创建后再补类型
// 阶段 4B 完成后，所有 BinaryOperator/UnaryOperator 都在 ActOnBinaryOperator/ActOnUnaryOperator
// 中直接设类型，ActOnExpr 的补设逻辑变为冗余
// 
// 简化为：
ExprResult Sema::ActOnExpr(Expr *E) {
  if (!E)
    return ExprResult::getInvalid();
  // 所有表达式应在 ActOn* 工厂方法中已设置类型
  // 此方法仅作为验证/兼容接口
  return ExprResult(E);
}
```

#### 4D-5. 更新注释和文档

- 删除 Sema.h 中 `/// Post-parse AST traversal` 等过时注释
- 更新 `driver.cpp` 步骤注释，体现新的编译流程

---

## 四、执行顺序与依赖关系

```
阶段 4A（Tier 1 基础类型）
  ├── 4A-1 DeclRefExpr    ← 最基础，后续 CallExpr/MemberExpr 的子表达式
  ├── 4A-5 CastExpr       ← 独立，不依赖其他修复
  ├── 4A-6 ArraySubscript ← 独立，依赖 DeclRefExpr
  ├── 4A-7 ConditionalExpr ← 修复 bug，依赖子表达式类型
  ├── 4A-3/4 MemberExpr   ← 依赖 DeclRefExpr
  └── 4A-2 CallExpr       ← 依赖 DeclRefExpr + FunctionDecl 类型链
      ↓
阶段 4B（Tier 2 + 移除防御跳过）
  ├── 4B-3/4 Sizeof/Alignof ← 独立
  ├── 4B-7~10 This/Throw/New/Delete ← 独立
  ├── 4B-1/2 BinaryOp/UnaryOp ← 移除防御跳过，依赖 4A 全部完成
  ├── 4B-5/6 NamedCast     ← 需扩展签名
  └── 4B-11 CXXConstructExpr ← 需扩展签名
      ↓
阶段 4C（Tier 3 高级特性）
  └── 各方法独立推进，标记 TODO
      ↓
阶段 4D（清除 ProcessAST — 对应原阶段2计划子阶段 2F）
  ├── 4D-1 删除 ASTVisitor + ProcessAST
  ├── 4D-2 删除 ActOnCXXNewExpr(CXXNewExpr*) + ActOnCXXDeleteExpr(CXXDeleteExpr*)
  ├── 4D-3 从 driver.cpp 移除 S.ProcessAST(TU) 调用
  ├── 4D-4 简化 ActOnExpr() 通用补设
  └── 4D-5 更新注释和文档
```

### 关键依赖链

1. **DeclRefExpr → MemberExpr → CallExpr**：DeclRefExpr 是最基础的引用表达式，必须最先修复
2. **Tier 1 全部 → BinaryOp/UnaryOp 防御移除**：只有子表达式类型完整后才能移除防御
3. **CastExpr 和其他修复无依赖**：可以并行推进

---

## 五、修改文件清单

| 文件 | 修改内容 | 阶段 |
|------|---------|------|
| `src/Sema/Sema.cpp` | 25+ 处 ActOn* 方法添加 `setType()` 调用 | 4A/4B/4C |
| `src/Sema/Sema.cpp` | 删除 `ASTVisitor`、`ProcessAST`、`ActOnCXXNewExpr(CXXNewExpr*)`、`ActOnCXXDeleteExpr(CXXDeleteExpr*)` | 4D |
| `src/Sema/Sema.cpp` | 简化 `ActOnExpr()` 通用类型补设逻辑 | 4D |
| `include/blocktype/Sema/Sema.h` | 扩展 `ActOnCXXNamedCastExpr`/`ActOnCXXConstructExpr` 签名；删除 `ProcessAST`/`ActOnCXXNewExpr(CXXNewExpr*)`/`ActOnCXXDeleteExpr(CXXDeleteExpr*)` 声明 | 4B/4D |
| `src/Parse/ParseExprCXX.cpp` | 适配扩展后的签名 | 4B |
| `include/blocktype/AST/ASTContext.h` | 新增 `getSizeType()` 方法（如不存在） | 4B |
| `tools/driver.cpp` | 删除 `S.ProcessAST(TU)` 调用 | 4D |

---

## 六、测试策略

### 6.1 基线测试

每阶段修改后运行全量测试：
```bash
cd build && ctest --output-on-failure
```
确保 662/662 测试全部通过。

### 6.2 新增测试用例

为每个修复的方法新增针对性测试：

| 方法 | 测试要点 |
|------|---------|
| `DeclRefExpr` | 验证 `int x; x` 的 ExprTy == int |
| `CallExpr` | 验证 `int f(); f()` 的 ExprTy == int |
| `MemberExpr` | 验证 `obj.field` 的 ExprTy == field 的类型 |
| `CastExpr` | 验证 `(float)x` 的 ExprTy == float |
| `ArraySubscriptExpr` | 验证 `arr[0]` 的 ExprTy == 元素类型 |
| `ConditionalExpr` | 验证 `cond ? a : b` 的 ExprTy == common type |
| `BinaryOp` | 验证 `a + b` 的 ExprTy == 算术结果类型 |
| `UnaryOp` | 验证 `*p` 的 ExprTy == 解引用类型 |
| `sizeof` | 验证 `sizeof(int)` 的 ExprTy == size_t |

### 6.3 回归保护

在阶段 4D 删除 ProcessAST 之前，可先在 `ProcessAST::visitExpr` 中添加**防御性断言**（仅 Debug 构建）作为中间验证步骤：

```cpp
void visitExpr(Expr *E) {
  if (!E) return;
  
  // 阶段 4A/4B 后：所有 Tier 1/2 表达式类型应在创建时已设置
  // 如果此处仍触发，说明有 ActOn* 方法遗漏了 setType
  assert(E->getType().isNull() && "ProcessAST should be dead code after 4D");
  
  // ... 原有处理（将在 4D 中完全删除）
}
```

**注意**：此断言仅在 4A/4B 完成后、4D 删除前短暂存在。4D 完成后 ProcessAST 整体删除，无需任何断言。

---

## 七、风险分析与回滚方案

### 7.1 风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 移除防御跳过后暴露隐藏 bug | 高 | 中 | 每处修改独立提交，出问题精确回滚 |
| ValueDecl::getType() 为空 | 中 | 高 | 添加空检查，仅在类型非空时设置 |
| FunctionType::getReturnType() 返回意外类型 | 中 | 中 | 添加类型检查和断言 |
| 扩展签名破坏调用点 | 低 | 中 | 编译器直接报错，修复容易 |
| 测试用例覆盖不足 | 中 | 中 | 逐步推进，先 Tier 1 验证稳定后再做 Tier 2 |

### 7.2 回滚策略

- 每个子阶段（4A-1, 4A-2, ...）独立 git commit
- 出问题时 `git revert` 单个 commit 即可回滚
- 防御性 `getType().isNull()` 检查保留为安全网（仅在修复后类型**应该**非空时才移除）

---

## 八、完成标准

1. ✅ 所有 Tier 1 方法（7 处）在创建节点时设置 `ExprTy`
2. ✅ 所有 Tier 2 方法（13 处）在创建节点时设置 `ExprTy`
3. ✅ Tier 3 方法标记 `TODO` 注释说明后续计划
4. ✅ **`ProcessAST` 完全删除**（包括 `ASTVisitor` 内部类）
5. ✅ **`ActOnCXXNewExpr(CXXNewExpr*)` / `ActOnCXXDeleteExpr(CXXDeleteExpr*)` 后处理方法删除**
6. ✅ **`driver.cpp` 不再调用 `S.ProcessAST(TU)`**
7. ✅ `ActOnExpr()` 简化为纯转发，不含类型补设逻辑
8. ✅ 662/662 测试全部通过
9. ✅ 编译流程变为 `Parse → CodeGen`，Sema 的工作内嵌于 Parser 解析过程中的 `ActOn*` 调用

### 架构最终状态

```
修改前（阶段 3 后）:
  Parser → Sema::ActOn*（创建节点但不设类型）→ Sema::ProcessAST（遍历 AST 补类型）→ CodeGen
  
修改后（阶段 4D 后）:
  Parser → Sema::ActOn*（创建节点并即时设类型）→ CodeGen
  ProcessAST: 已删除
  ActOnCXXNewExpr(CXXNewExpr*): 已删除（由 ActOnCXXNewExprFactory 替代）
  ActOnCXXDeleteExpr(CXXDeleteExpr*): 已删除（由 ActOnCXXDeleteExprFactory 替代）
```
