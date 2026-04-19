# Phase 7 详细接口规划

> **目的：** 为Phase 7的所有Task预先定义完整的API接口，避免Phase 3的困境
> **原则：** 接口先行、预置API、参照Clang、避免随机编码
> **更新日期：** 2026-04-19

---

## 📋 总览

本文件详细列出Phase 7所有Stage/Task需要创建或修改的头文件、类、方法、枚举、AST节点、诊断ID等。

**关键提醒：**
- ✅ 每个Task开始前，必须先完成本节列出的接口定义
- ✅ 即使实现是空壳，接口必须完整
- ✅ 为后续Task预置的方法签名必须包含
- ❌ 禁止在实现阶段临时发明API

---

## Stage 7.1 — C++23 P1 特性

### Task 7.1.1 Deducing this (P0847R7)

#### 需要修改的头文件

**1. `include/blocktype/AST/Decl.h`**

```cpp
// 在 FunctionDecl 类中添加（约第150行附近）
class FunctionDecl : public ValueDecl {
  // ... 现有字段 ...
  
  // ⚠️ P7.1.1 新增：Deducing this 支持
  ParmVarDecl *ExplicitObjectParam = nullptr;
  bool HasExplicitObjectParam = false;
  
public:
  // ⚠️ P7.1.1 新增方法
  bool hasExplicitObjectParam() const { return HasExplicitObjectParam; }
  ParmVarDecl *getExplicitObjectParam() const { return ExplicitObjectParam; }
  void setExplicitObjectParam(ParmVarDecl *P) {
    ExplicitObjectParam = P;
    HasExplicitObjectParam = (P != nullptr);
  }
  
  /// 获取有效的 this 类型（考虑 deducing this）
  /// 
  /// 如果有 explicit object parameter，返回其类型
  /// 否则返回传统的 this 指针类型
  QualType getThisType(ASTContext &Ctx) const;
};
```

**2. `include/blocktype/Sema/SemaCXX.h`（新建）**

```cpp
#pragma once
#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"

namespace blocktype {

/// SemaCXX - C++ 特有语义分析
/// 
/// ⚠️ P7新增：集中处理 C++23/C++26 特性的语义分析
/// 包括：Deducing this, static operator, Contracts 等
class SemaCXX {
  Sema &S;
  
public:
  explicit SemaCXX(Sema &SemaRef) : S(SemaRef) {}
  
  //===------------------------------------------------------------------===//
  // Deducing this (P0847R7)
  //===------------------------------------------------------------------===//
  
  /// 检查 Deducing this 函数的合法性
  /// 
  /// **规则**：
  /// - 只能用于非静态成员函数
  /// - 不能与 virtual 同时使用
  /// - 不能有默认参数
  /// - 类型必须是类类型的引用或值
  /// 
  /// @param FD 函数声明
  /// @param Param 显式对象参数
  /// @param ParamLoc 参数位置（用于诊断）
  /// @return true 如果合法
  bool CheckExplicitObjectParameter(FunctionDecl *FD, 
                                     ParmVarDecl *Param,
                                     SourceLocation ParamLoc);
  
  /// 推导 explicit object parameter 的类型
  /// 
  /// 根据调用对象的类型和值类别，推导 Self 的实际类型
  /// 
  /// @param ObjectType 调用对象的类型
  /// @param ParamType 参数声明的类型
  /// @param VK 值类别（lvalue/rvalue）
  /// @return 推导后的类型
  QualType DeduceExplicitObjectType(QualType ObjectType,
                                     QualType ParamType,
                                     ExprValueKind VK);
  
  //===------------------------------------------------------------------===//
  // Static operator (P1169R4, P2589R1) - 预置接口
  //===------------------------------------------------------------------===//
  
  /// 检查 static operator 的合法性
  bool CheckStaticOperator(CXXMethodDecl *MD, SourceLocation Loc);
  
  //===------------------------------------------------------------------===//
  // Contracts (P2900R14) - 预置接口
  //===------------------------------------------------------------------===//
  
  /// 检查 Contract 条件
  bool CheckContractCondition(Expr *Cond, SourceLocation Loc);
};

} // namespace blocktype
```

**3. `include/blocktype/CodeGen/CGCXX.h`**

```cpp
// 在 CodeGenFunction 类中添加（约第100行附近）
class CodeGenFunction {
  // ... 现有字段和方法 ...
  
public:
  // ⚠️ P7.1.1 新增方法
  
  /// 生成 explicit object parameter 的调用代码
  /// 
  /// 当函数有 deducing this 时，对象作为第一个参数传递
  /// 而非通过隐含的 this 指针
  void EmitExplicitObjectParameterCall(CXXMemberCallExpr *E,
                                        llvm::Value *ObjectArg);
  
private:
  /// 调整对象以匹配 explicit object parameter 的类型
  llvm::Value *AdjustObjectForExplicitParam(llvm::Value *Obj,
                                             QualType ParamType,
                                             ExprValueKind VK);
};
```

#### 需要添加的诊断ID

**文件：** `include/blocktype/Basic/DiagnosticSemaKinds.def`

```cpp
//===----------------------------------------------------------------------===//
// Deducing this diagnostics (P0847R7)
//===----------------------------------------------------------------------===//

DIAG(err_explicit_object_param_static, Error,
  "explicit object parameter cannot be used in static member function",
  "显式对象参数不能用于静态成员函数")

DIAG(err_explicit_object_param_virtual, Error,
  "explicit object parameter cannot be used with virtual function",
  "显式对象参数不能与虚函数同时使用")

DIAG(err_explicit_object_param_default_arg, Error,
  "explicit object parameter cannot have default argument",
  "显式对象参数不能有默认参数")

DIAG(err_explicit_object_param_type, Error,
  "explicit object parameter type must be a reference or value of class type",
  "显式对象参数类型必须是类类型的引用或值")

DIAG(warn_explicit_object_param_unused, Warning,
  "explicit object parameter '%0' is unused",
  "显式对象参数 '%0' 未使用")
```

#### 需要修改的源文件清单

| 文件 | 修改内容 | 行数估算 |
|------|---------|---------|
| `src/AST/Decl.cpp` | 实现 `getThisType()` | ~20行 |
| `src/Parse/ParseDecl.cpp` | 解析 explicit object parameter | ~80行 |
| `src/Sema/SemaCXX.cpp`（新建） | 实现 `CheckExplicitObjectParameter` | ~100行 |
| `src/Sema/Sema.cpp` | 集成检查逻辑 | ~10行 |
| `src/CodeGen/CGCXX.cpp` | 实现参数调整逻辑 | ~60行 |
| `tests/cpp23/deducing_this.cpp` | 测试用例 | ~100行 |

---

### Task 7.1.2 decay-copy 表达式 (P0849R8)

#### 需要新增的AST节点

**文件：** `include/blocktype/AST/Expr.h`

```cpp
// 在 Expr 类层次中添加（约第800行附近）

/// DecayCopyExpr - decay-copy 表达式 auto(expr) 或 auto{expr}
/// 
/// 语义：对 expr 执行 decay（去除引用、cv限定符），然后创建临时对象
class DecayCopyExpr : public Expr {
  Expr *SubExpr;
  bool IsDirectInit;  // true = auto{expr}, false = auto(expr)
  
public:
  DecayCopyExpr(SourceLocation Loc, Expr *Sub, bool DirectInit)
      : Expr(Loc, QualType(), ExprValueKind::VK_PRValue),  // 类型稍后设置
        SubExpr(Sub), IsDirectInit(DirectInit) {}
  
  Expr *getSubExpr() const { return SubExpr; }
  bool isDirectInit() const { return IsDirectInit; }
  
  NodeKind getKind() const override { return NodeKind::DecayCopyExprKind; }
  
  static bool classof(const ASTNode *N) {
    return N->getKind() == NodeKind::DecayCopyExprKind;
  }
};
```

**文件：** `include/blocktype/AST/Stmt.h`（如果需要）

```cpp
// 在 NodeKind 枚举中添加
enum class NodeKind {
  // ... 现有值 ...
  
  // ⚠️ P7.1.2 新增
  DecayCopyExprKind,
};
```

#### 需要修改的Parser

**文件：** `src/Parse/ParseExpr.cpp`

```cpp
// 在 parsePostfixExpression 或 parsePrimaryExpression 中添加

Expr *Parser::parseDecayCopyExpression() {
  // 检测语法：auto ( expr ) 或 auto { expr }
  // 
  // 伪代码：
  // 1. 消费 'auto' 关键字
  // 2. 检测 '(' 或 '{'
  // 3. 解析子表达式
  // 4. 创建 DecayCopyExpr
  // 5. Sema 进行类型推导
  
  SourceLocation AutoLoc = Tok.getLocation();
  consumeToken(); // 消费 'auto'
  
  bool IsDirectInit = Tok.is(TokenKind::l_brace);
  expectAndConsume(IsDirectInit ? TokenKind::l_brace : TokenKind::l_paren);
  
  Expr *Sub = parseExpression();
  
  expectAndConsume(IsDirectInit ? TokenKind::r_brace : TokenKind::r_paren);
  
  return Actions.ActOnDecayCopyExpr(AutoLoc, Sub, IsDirectInit).get();
}
```

#### 需要新增的Sema方法

**文件：** `include/blocktype/Sema/Sema.h`

```cpp
class Sema {
public:
  // ⚠️ P7.1.2 新增
  
  /// 处理 decay-copy 表达式
  /// 
  /// 1. 对 SubExpr 执行 decay（去除引用、顶层cv）
  /// 2. 创建临时对象
  /// 3. 设置 DecayCopyExpr 的类型
  ExprResult ActOnDecayCopyExpr(SourceLocation AutoLoc,
                                 Expr *SubExpr,
                                 bool IsDirectInit);
};
```

**文件：** `include/blocktype/Sema/TypeCheck.h`（如果不存在则新建）

```cpp
#pragma once
#include "blocktype/AST/Type.h"

namespace blocktype {

/// TypeCheck - 类型检查工具类
/// 
/// ⚠️ P7预置：提供通用的类型检查和转换功能
class TypeCheck {
public:
  /// 对类型执行 decay
  /// 
  /// 去除：
  /// - 引用（lvalue/rvalue）
  /// - 顶层 const/volatile
  /// - 数组到指针的转换
  /// - 函数到指针的转换
  /// 
  /// @param T 原始类型
  /// @param Ctx AST上下文
  /// @return decay后的类型
  static QualType Decay(QualType T, ASTContext &Ctx);
  
  /// 检查类型是否可以复制初始化
  static bool CanCopyInitialize(QualType From, QualType To, ASTContext &Ctx);
  
  /// 检查类型是否可以直接初始化
  static bool CanDirectInitialize(QualType From, QualType To, ASTContext &Ctx);
};

} // namespace blocktype
```

#### 诊断ID

**文件：** `include/blocktype/Basic/DiagnosticSemaKinds.def`

```cpp
//===----------------------------------------------------------------------===//
// decay-copy diagnostics (P0849R8)
//===----------------------------------------------------------------------===//

DIAG(err_decay_copy_non_copyable, Error,
  "cannot decay-copy expression of non-copyable type '%0'",
  "无法decay-copy不可复制类型 '%0' 的表达式")

DIAG(warn_decay_copy_redundant, Warning,
  "decay-copy is redundant for prvalue of type '%0'",
  "对于类型为 '%0' 的纯右值，decay-copy是冗余的")
```

---

### Task 7.1.3 static operator (P1169R4, P2589R1)

#### 需要修改的AST节点

**文件：** `include/blocktype/AST/Decl.h`

```cpp
// 在 CXXMethodDecl 或 FunctionDecl 中添加

class CXXMethodDecl : public FunctionDecl {
  // ... 现有字段 ...
  
  // ⚠️ P7.1.3 新增
  bool IsStaticOperator = false;
  
public:
  bool isStaticOperator() const { return IsStaticOperator; }
  void setStaticOperator(bool V) { IsStaticOperator = V; }
  
  /// 检查是否是 static operator() 或 static operator[]
  bool isStaticCallOperator() const;
  bool isStaticSubscriptOperator() const;
};
```

#### Parser修改

**文件：** `src/Parse/ParseDeclCXX.cpp`

```cpp
// 在 parseCXXMemberDeclarator 中检测 static operator

// 伪代码：
// if (Tok.is(TokenKind::kw_static)) {
//   consumeToken();
//   if (Tok.isOperatorKeyword()) {
//     // 解析 static operator
//     MD->setStaticOperator(true);
//   }
// }
```

#### Sema检查

**文件：** `include/blocktype/Sema/SemaCXX.h`（已在7.1.1中声明）

```cpp
// CheckStaticOperator 方法实现要点：
// 1. static operator 不能有 this 指针
// 2. static operator() 可以像普通函数一样调用：S::operator()(args)
// 3. static operator[] 允许多个参数
```

---

### Task 7.1.4 [[assume]] 属性 (P1774R8)

#### 需要新增的AST节点

**文件：** `include/blocktype/AST/Attr.h`（新建）

```cpp
#pragma once
#include "blocktype/AST/ASTNode.h"
#include "blocktype/AST/Expr.h"

namespace blocktype {

/// AssumeAttr - [[assume(condition)]] 属性
class AssumeAttr : public Attr {
  Expr *Condition;
  
public:
  AssumeAttr(SourceLocation Loc, Expr *Cond)
      : Attr(Loc), Condition(Cond) {}
  
  Expr *getCondition() const { return Condition; }
  
  static bool classof(const ASTNode *N) {
    return N->getKind() == NodeKind::AssumeAttrKind;
  }
};

} // namespace blocktype
```

#### CodeGen支持

**文件：** `include/blocktype/CodeGen/CGAttrs.h`（新建）

```cpp
#pragma once
#include "llvm/IR/Instructions.h"

namespace blocktype {

class CodeGenFunction;
class AssumeAttr;

/// CGAttrs - 属性代码生成
class CGAttrs {
public:
  /// 生成 llvm.assume intrinsic
  static void EmitAssumeAttr(CodeGenFunction &CGF, 
                              const AssumeAttr *Attr);
};

} // namespace blocktype
```

---

## Stage 7.2 — 静态反射完善

### Task 7.2.1 reflexpr 关键字完善

#### 需要新增的AST节点

**文件：** `include/blocktype/AST/Expr.h`

```cpp
/// ReflexprExpr - reflexpr(type_or_expr) 表达式
class ReflexprExpr : public Expr {
  enum OperandKind {
    OK_Type,
    OK_Expression
  };
  
  union {
    QualType ReflectedType;
    Expr *ReflectedExpr;
  };
  OperandKind Kind;
  
public:
  ReflexprExpr(SourceLocation Loc, QualType T)
      : Expr(Loc, QualType(), ExprValueKind::VK_PRValue),
        ReflectedType(T), Kind(OK_Type) {}
  
  ReflexprExpr(SourceLocation Loc, Expr *E)
      : Expr(Loc, QualType(), ExprValueKind::VK_PRValue),
        ReflectedExpr(E), Kind(OK_Expression) {}
  
  bool reflectsType() const { return Kind == OK_Type; }
  bool reflectsExpression() const { return Kind == OK_Expression; }
  
  QualType getReflectedType() const { return ReflectedType; }
  Expr *getReflectedExpr() const { return ReflectedExpr; }
  
  NodeKind getKind() const override { return NodeKind::ReflexprExprKind; }
};
```

#### 反射类型系统

**文件：** `include/blocktype/AST/ReflectionTypes.h`（新建）

```cpp
#pragma once
#include "blocktype/AST/Type.h"

namespace blocktype {
namespace meta {

/// info - 反射信息类型（对应 std::meta::info）
/// 
/// 这是一个不透明类型，实际存储指向 AST 节点的指针
class InfoType : public Type {
  void *Reflectee;  // 指向被反射的 AST 节点
  enum ReflecteeKind {
    RK_Type,
    RK_Decl,
    RK_Expr
  } Kind;
  
public:
  InfoType(void *R, ReflecteeKind K)
      : Type(TypeClass::MetaInfo), Reflectee(R), Kind(K) {}
  
  void *getReflectee() const { return Reflectee; }
  ReflecteeKind getReflecteeKind() const { return Kind; }
  
  static bool classof(const Type *T) {
    return T->getTypeClass() == TypeClass::MetaInfo;
  }
};

} // namespace meta
} // namespace blocktype
```

**文件：** `include/blocktype/AST/Type.h`

```cpp
// 在 TypeClass 枚举中添加
enum class TypeClass {
  // ... 现有值 ...
  
  // ⚠️ P7.2.1 新增
  MetaInfo,  // 反射信息类型
};
```

---

## Stage 7.3 — Contracts

### Task 7.3.1 Contract 属性

#### 需要新增的AST节点

**文件：** `include/blocktype/AST/Attr.h`

```cpp
// 添加 Contract 相关枚举和类

/// Contract 种类
enum class ContractKind {
  Pre,    // [[pre: condition]]
  Post,   // [[post: condition]]
  Assert  // [[assert: condition]]
};

/// Contract 检查模式
enum class ContractMode {
  Abort,     // 失败时终止
  Continue,  // 失败时继续
  Observe,   // 观察模式
  Enforce    // 强制检查
};

/// ContractAttr - Contract 属性
class ContractAttr : public Attr {
  ContractKind Kind;
  Expr *Condition;
  ContractMode Mode;
  
public:
  ContractAttr(SourceLocation Loc, ContractKind K, 
               Expr *Cond, ContractMode M = ContractMode::Enforce)
      : Attr(Loc), Kind(K), Condition(Cond), Mode(M) {}
  
  ContractKind getKind() const { return Kind; }
  Expr *getCondition() const { return Condition; }
  ContractMode getMode() const { return Mode; }
  
  static bool classof(const ASTNode *N) {
    return N->getKind() == NodeKind::ContractAttrKind;
  }
};
```

---

## Stage 7.4 — C++26 P1/P2 特性

### Task 7.4.1 `= delete("reason")`

#### 需要修改的AST节点

**文件：** `include/blocktype/AST/Decl.h`

```cpp
class FunctionDecl : public ValueDecl {
  // ... 现有字段 ...
  
  // ⚠️ P7.4.1 新增
  StringLiteral *DeletedReason = nullptr;
  
public:
  bool hasDeletedReason() const { return DeletedReason != nullptr; }
  StringLiteral *getDeletedReason() const { return DeletedReason; }
  void setDeletedReason(StringLiteral *S) { DeletedReason = S; }
};
```

#### 诊断增强

**文件：** `include/blocktype/Basic/DiagnosticSemaKinds.def`

```cpp
DIAG(err_use_of_deleted_function_with_reason, Error,
  "call to deleted function '%0': %1",
  "调用已删除函数 '%0'：%1")
```

---

## 📊 完整文件清单

### 需要新建的文件

| 文件 | 服务于 | 优先级 |
|------|--------|--------|
| `include/blocktype/Sema/SemaCXX.h` | 7.1.1, 7.1.3, 7.3 | P0 |
| `include/blocktype/Sema/TypeCheck.h` | 7.1.2 | P0 |
| `include/blocktype/AST/ReflectionTypes.h` | 7.2.1 | P1 |
| `include/blocktype/CodeGen/CGAttrs.h` | 7.1.4 | P1 |
| `src/Sema/SemaCXX.cpp` | 7.1.1, 7.1.3, 7.3 | P0 |
| `src/Sema/TypeCheck.cpp` | 7.1.2 | P0 |
| `src/CodeGen/CGAttrs.cpp` | 7.1.4 | P1 |
| `tests/cpp23/*.cpp` | 7.1 | P0 |
| `tests/cpp26/*.cpp` | 7.4 | P1 |

### 需要修改的文件

| 文件 | 修改内容 | Task |
|------|---------|------|
| `include/blocktype/AST/Decl.h` | FunctionDecl/CXXMethodDecl 扩展 | 7.1.1, 7.1.3, 7.4.1 |
| `include/blocktype/AST/Expr.h` | 新增 DecayCopyExpr, ReflexprExpr | 7.1.2, 7.2.1 |
| `include/blocktype/AST/Attr.h` | 新增 AssumeAttr, ContractAttr | 7.1.4, 7.3.1 |
| `include/blocktype/AST/Type.h` | TypeClass 枚举扩展 | 7.2.1 |
| `include/blocktype/AST/Stmt.h` | NodeKind 枚举扩展 | 全部 |
| `include/blocktype/CodeGen/CGCXX.h` | 新增方法声明 | 7.1.1 |
| `include/blocktype/Basic/DiagnosticSemaKinds.def` | 新增诊断ID | 全部 |
| `src/Parse/ParseDecl*.cpp` | 解析新语法 | 7.1.1, 7.1.2, 7.1.3 |
| `src/Sema/Sema.cpp` | 集成检查逻辑 | 全部 |
| `src/CodeGen/CGCXX.cpp` | CodeGen实现 | 7.1.1 |

---

## ⚠️ 实施检查清单

在开始每个Task之前，必须确认：

### Task 7.1.1 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 FunctionDecl 已添加 explicit object parameter 字段
- [ ] `include/blocktype/Sema/SemaCXX.h` 已创建并声明 CheckExplicitObjectParameter
- [ ] `include/blocktype/CodeGen/CGCXX.h` 已添加 EmitExplicitObjectParameterCall
- [ ] `include/blocktype/Basic/DiagnosticSemaKinds.def` 已添加相关诊断ID

### Task 7.1.2 开始前
- [ ] `include/blocktype/AST/Expr.h` 中已添加 DecayCopyExpr 类
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnDecayCopyExpr
- [ ] `include/blocktype/Sema/TypeCheck.h` 已创建并声明 Decay 方法
- [ ] `include/blocktype/AST/Stmt.h` 中 NodeKind 已添加 DecayCopyExprKind

### Task 7.1.3 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 CXXMethodDecl 已添加 IsStaticOperator 字段
- [ ] `include/blocktype/Sema/SemaCXX.h` 中已声明 CheckStaticOperator

### Task 7.1.4 开始前
- [ ] `include/blocktype/AST/Attr.h` 已创建并声明 AssumeAttr
- [ ] `include/blocktype/CodeGen/CGAttrs.h` 已创建并声明 EmitAssumeAttr

### Task 7.2.1 开始前
- [ ] `include/blocktype/AST/Expr.h` 中已添加 ReflexprExpr 类
- [ ] `include/blocktype/AST/ReflectionTypes.h` 已创建并声明 InfoType
- [ ] `include/blocktype/AST/Type.h` 中 TypeClass 已添加 MetaInfo

### Task 7.3.1 开始前
- [ ] `include/blocktype/AST/Attr.h` 中已添加 ContractAttr, ContractKind, ContractMode
- [ ] `include/blocktype/Sema/SemaCXX.h` 中已声明 CheckContractCondition

### Task 7.4.1 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 FunctionDecl 已添加 DeletedReason 字段
- [ ] `include/blocktype/Basic/DiagnosticSemaKinds.def` 已添加 err_use_of_deleted_function_with_reason

---

## 🆕 新增：核查发现的未实现特性（2026-04-19）

> **背景：** 在核查Phase 7特性状态时，发现3项之前误标为“已实现”的特性实际未实现。
> **影响：** 需要将这3项加入Phase 7计划或后续Phase。
> **优先级：** P2（次要优先级）

### Task 7.5.4 多维 `operator[]` (P2128R6)

**目标：** 实现C++23的多维下标运算符，支持 `obj[i, j, k]` 语法

**⚠️ 接口预置清单：**

**1. `include/blocktype/AST/Expr.h`**

```cpp
/// MultiDimensionalSubscriptExpr - 多维下标表达式 obj[i, j, k]
class MultiDimensionalSubscriptExpr : public Expr {
  Expr *Base;  // 基表达式
  llvm::SmallVector<Expr *, 4> Indices;  // 多个索引
  
public:
  MultiDimensionalSubscriptExpr(SourceLocation Loc, Expr *Base,
                                 llvm::ArrayRef<Expr *> Indices)
      : Expr(Loc), Base(Base), Indices(Indices.begin(), Indices.end()) {}
  
  Expr *getBase() const { return Base; }
  llvm::ArrayRef<Expr *> getIndices() const { return Indices; }
  unsigned getNumIndices() const { return Indices.size(); }
  
  NodeKind getKind() const override { 
    return NodeKind::MultiDimensionalSubscriptExprKind; 
  }
  
  static bool classof(const ASTNode *N) {
    return N->getKind() == NodeKind::MultiDimensionalSubscriptExprKind;
  }
};
```

**2. `include/blocktype/AST/Stmt.h`**

```cpp
enum class NodeKind {
  // ... 现有值 ...
  
  // ⚠️ P7.5.4 新增
  MultiDimensionalSubscriptExprKind,
};
```

**3. `include/blocktype/Sema/Sema.h`**

```cpp
class Sema {
public:
  /// 处理多维下标表达式
  /// 
  /// 检查：
  /// - operator[] 是否接受多个参数
  /// - 类型匹配
  ExprResult ActOnMultiDimensionalSubscript(SourceLocation Loc,
                                             Expr *Base,
                                             llvm::ArrayRef<Expr *> Indices);
};
```

**4. `include/blocktype/Basic/DiagnosticSemaKinds.def`**

```cpp
DIAG(err_multidimensional_subscript_no_operator, Error,
  "type '%0' does not support multidimensional subscript operator[]",
  "类型 '%0' 不支持多维下标运算符[]")

DIAG(err_multidimensional_subscript_arg_count, Error,
  "operator[] expects %0 arguments but %1 were provided",
  "operator[] 期望 %0 个参数但提供了 %1 个")
```

**开发要点：**
- Parser需要解析逗号分隔的多个索引
- Sema需要查找接受多个参数的operator[]
- CodeGen生成函数调用

**验收标准：**
- [ ] 能解析 `obj[i, j, k]` 语法
- [ ] 能查找到多参数operator[]
- [ ] 生成正确的函数调用
- [ ] 至少3个测试用例

---

### Task 7.5.5 `#warning` 预处理指令 (P2437R1)

**目标：** 实现#warning预处理指令，输出警告但不终止编译

**⚠️ 接口预置清单：**

**1. `include/blocktype/Lex/Preprocessor.h`**

```cpp
// 在 DirectiveKind 枚举中添加
enum class DirectiveKind {
  // ... 现有值 ...
  
  // ⚠️ P7.5.5 新增
  Warning,  // #warning
};
```

**2. `src/Lex/Preprocessor.cpp`**

```cpp
// 在 handleDirective 中添加
} else if (DirectiveName == "warning") {
  Directive = DirectiveKind::Warning;
}

// 实现 #warning 处理
void Preprocessor::handleWarningDirective(SourceLocation Loc,
                                           llvm::StringRef Message) {
  // 输出警告消息，但不终止编译
  Diags.report(Loc, DiagLevel::Warning, Message.str());
}
```

**3. `include/blocktype/Basic/DiagnosticLexKinds.def`**（如果不存在则新建）

```cpp
DIAG(warn_pp_warning_directive, Warning,
  "#warning: %0",
  "#warning：%0")
```

**开发要点：**
- Lexer识别#warning关键字
- Preprocessor处理并输出警告
- 与#error的区别：不终止编译

**验收标准：**
- [ ] 能解析#warning指令
- [ ] 输出警告消息
- [ ] 编译继续执行
- [ ] 至少2个测试用例

---

### Task 7.5.6 `Z`/`z` 字面量后缀 (P0330R8)

**目标：** 实现size_t类型的字面量后缀 `z`/`Z`

**⚠️ 接口预置清单：**

**1. `include/blocktype/Lex/Lexer.h`**

```cpp
// 在整数/浮点数解析中添加后缀处理
bool isSizeTSuffix(char C) const {
  return C == 'z' || C == 'Z';
}
```

**2. `src/Lex/Lexer.cpp`**

```cpp
// 在 lexNumericConstant 或相关方法中
if (isSizeTSuffix(*BufferPtr)) {
  ++BufferPtr;
  // 标记为 size_t 类型
  LiteralInfo.HasSizeTSuffix = true;
}
```

**3. `include/blocktype/AST/Expr.h`**

```cpp
// IntegerLiteral 或 FloatingLiteral 添加字段
class IntegerLiteral : public Expr {
  // ... 现有字段 ...
  
  // ⚠️ P7.5.6 新增
  bool HasSizeTSuffix = false;
  
public:
  bool hasSizeTSuffix() const { return HasSizeTSuffix; }
  void setSizeTSuffix(bool V) { HasSizeTSuffix = V; }
};
```

**4. `include/blocktype/Sema/Sema.h`**

```cpp
class Sema {
public:
  /// 处理带 z/Z 后缀的字面量
  /// 
  /// 将字面量类型设置为 size_t (std::size_t)
  ExprResult ActOnIntegerLiteralWithSizeTSuffix(SourceLocation Loc,
                                                 llvm::APSInt Value);
};
```

**5. `include/blocktype/Basic/DiagnosticSemaKinds.def`**

```cpp
DIAG(warn_size_t_suffix_overflow, Warning,
  "integer literal with 'z' suffix overflows size_t",
  "带 'z' 后缀的整数字面量溢出 size_t")
```

**开发要点：**
- Lexer识别z/Z后缀
- Sema将类型设置为size_t
- 检查溢出
- CodeGen生成正确的类型

**验收标准：**
- [ ] 能解析 `123z` 和 `456Z`
- [ ] 类型为 size_t
- [ ] 溢出时给出警告
- [ ] 至少3个测试用例

---

## 🎯 总结

**核心原则重申：**
1. ✅ **接口先行**：先写头文件，再写实现
2. ✅ **预置API**：为后续Task预留方法签名
3. ✅ **完整枚举**：所有 enum/classof 必须完整
4. ✅ **诊断完备**：所有错误/警告消息预先定义
5. ❌ **禁止临时发明**：不允许在实现阶段创造新的API

**参照资源：**
- Clang 源码：`clang/include/clang/AST/`, `clang/lib/Sema/`, `clang/lib/CodeGen/`
- C++ 标准提案：P0847R7, P0849R8, P1169R4, P1774R8, P2900R14 等
- BlockType 现有架构：`docs/ARCHITECTURE.md`

**下一步行动：**
1. 按本文件清单逐个创建/修改头文件
2. 确保编译通过（即使实现是空壳）
3. 再开始实现 .cpp 文件
4. 最后编写测试

---

*文档维护者：AI Assistant*
*最后更新：2026-04-19*
