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

  /// ~~EmitExplicitObjectParameterCall~~ 已内联到 CodeGenExpr.cpp EmitCallExpr 中。
  /// ~~AdjustObjectForExplicitParam~~ 已内联到 CodeGenExpr.cpp EmitCallExpr 中。

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

### Task 7.4.2 占位符变量 `_` (P2169R4)

#### 需要修改的AST/语义分析

**文件：** `include/blocktype/Sema/Sema.h`

```cpp
class Sema {
public:
  // ⚠️ P7.4.2 新增

  /// 处理占位符变量声明 `_`
  ///
  /// **规则（参照 Clang Sema::ActOnVarDecl）**：
  /// - `_` 不加入符号表（每次 `_` 都是新变量）
  /// - 不产生 "unused variable" 警告
  /// - 允许在同一作用域多次声明 `auto _ = expr`
  ///
  /// **Clang 参考**：
  /// - `clang/lib/Sema/SemaDecl.cpp` 中 `isPlaceholderVarDecl()`
  /// - `clang/include/clang/AST/Decl.h` 中 `VarDecl::isPlaceholderVar()`
  VarDecl *ActOnPlaceholderVarDecl(SourceLocation Loc, QualType Type,
                                    Expr *Init);

  /// 检查标识符是否为占位符 `_`
  static bool isPlaceholderIdentifier(llvm::StringRef Name) {
    return Name == "_";
  }
};
```

**文件：** `include/blocktype/AST/Decl.h`

```cpp
class VarDecl : public ValueDecl {
  // ... 现有字段 ...

  // ⚠️ P7.4.2 新增
  bool IsPlaceholder = false;  // 占位符变量（C++26 _）

public:
  bool isPlaceholder() const { return IsPlaceholder; }
  void setPlaceholder(bool V) { IsPlaceholder = V; }
};
```

#### 词法/语法分析

**文件：** `src/Parse/ParseDecl.cpp`

```cpp
// 在 parseSimpleDeclaration 中添加占位符检测：
// 伪代码：
// if (Tok.is(TokenKind::identifier) && Tok.getText() == "_") {
//   // 不查找符号表，直接创建占位符 VarDecl
//   auto *VD = Actions.ActOnPlaceholderVarDecl(Tok.getLocation(), Type, Init);
//   return VD;
// }
```

#### CodeGen

**文件：** `src/CodeGen/CodeGenStmt.cpp`

```cpp
// 在 EmitDeclStmt 中：
// if (auto *VD = dyn_cast<VarDecl>(D)) {
//   if (VD->isPlaceholder()) {
//     // 生成临时 alloca 但不绑定名称
//     // 如果是 struct 类型，仍然需要调用析构函数
//   }
// }
```

#### 诊断ID

**文件：** `include/blocktype/Basic/DiagnosticSemaKinds.def`

```cpp
DIAG(warn_placeholder_var_with_linkage, Warning,
  "placeholder variable '_' declared with external linkage",
  "占位符变量 '_' 声明了外部链接")

DIAG(err_placeholder_var_not_auto, Error,
  "placeholder variable '_' must have an auto type or be initialized",
  "占位符变量 '_' 必须有 auto 类型或被初始化")
```

#### 实施检查清单

- [ ] `include/blocktype/AST/Decl.h` 中 VarDecl 已添加 IsPlaceholder 字段
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnPlaceholderVarDecl
- [ ] Parser 中 `_` 标识符特殊处理
- [ ] CodeGen 正确处理占位符变量

---

### Task 7.4.3 结构化绑定扩展 (P0963R3, P1061R10)

#### 需要新增的AST节点

**文件：** `include/blocktype/AST/Decl.h`

```cpp
/// BindingDecl — 结构化绑定声明 (C++17 `auto [x, y] = expr`，C++26 扩展)
///
/// **Clang 参考**：
/// - `clang/include/clang/AST/Decl.h` 中 `BindingDecl`
/// - `clang/lib/Sema/SemaDeclCXX.cpp` 中 `BuildDecompositionDecl`
///
/// **C++26 扩展 (P0963R3)**：
/// - 支持 `if (auto [x, y] = getPair())` 条件中使用
///
/// **C++26 扩展 (P1061R10)**：
/// - 支持 `template <typename... Ts> auto [...ns] = tuple;` 引入包
class BindingDecl : public ValueDecl {
  Expr *BindingExpr;  // 绑定的表达式（如 get<N>(tuple)）
  unsigned BindingIndex;  // 在结构化绑定中的索引

  // ⚠️ P7.4.3 C++26 扩展
  bool IsPackExpansion = false;  // 是否为包展开 (P1061R10)

public:
  BindingDecl(SourceLocation Loc, llvm::StringRef Name,
              Expr *BE, unsigned Idx)
      : ValueDecl(Decl::BindingDeclKind, Loc, Name, QualType()),
        BindingExpr(BE), BindingIndex(Idx) {}

  Expr *getBindingExpr() const { return BindingExpr; }
  unsigned getBindingIndex() const { return BindingIndex; }

  // C++26: 包展开
  bool isPackExpansion() const { return IsPackExpansion; }
  void setPackExpansion(bool V) { IsPackExpansion = V; }

  NodeKind getKind() const override { return NodeKind::BindingDeclKind; }

  static bool classof(const ASTNode *N) {
    return N->getKind() == NodeKind::BindingDeclKind;
  }
};
```

**文件：** `include/blocktype/AST/Stmt.h`（NodeKinds.def 中新增）

```cpp
// NodeKinds.def 中新增
DECL(BindingDecl, ValueDecl)
```

#### 条件中的结构化绑定 (P0963R3)

**文件：** `include/blocktype/AST/Stmt.h`

```cpp
// IfStmt 扩展：允许条件为结构化绑定声明
// 无需新增字段——通过 CondVar 已支持 VarDecl
// BindingDecl 声明通过 DeclStmt 包裹

// 但需要 Sema 支持：
```

**文件：** `include/blocktype/Sema/Sema.h`

```cpp
class Sema {
public:
  // ⚠️ P7.4.3 新增

  /// 创建结构化绑定声明组
  ///
  /// `auto [a, b, c] = expr` → 创建多个 BindingDecl
  ///
  /// **规则**：
  /// - 绑定数量必须与 std::tuple_size 匹配
  /// - 每个绑定通过 std::get<N> 提取
  /// - 绑定变量的类型为 auto 推导
  ///
  /// **Clang 参考**：
  /// - `clang/lib/Sema/SemaDeclCXX.cpp` 中 `BuildDecompositionDecl()`
  /// - `clang/include/clang/AST/DeclCXX.h` 中 `DecompositionDecl`
  DeclGroupRef ActOnDecompositionDecl(SourceLocation Loc,
                                       llvm::ArrayRef<llvm::StringRef> Names,
                                       Expr *Init);

  /// 检查结构化绑定是否可用于条件表达式 (P0963R3)
  bool CheckBindingCondition(llvm::ArrayRef<BindingDecl *> Bindings,
                              SourceLocation Loc);
};
```

#### 诊断ID

```cpp
DIAG(err_decomp_decl_not_tuple, Error,
  "cannot decompose non-tuple type '%0'",
  "无法分解非元组类型 '%0'")

DIAG(err_decomp_decl_wrong_count, Error,
  "decomposition declaration requires %0 bindings but %1 were provided",
  "分解声明需要 %0 个绑定但提供了 %1 个")

DIAG(err_decomp_decl_in_condition, Error,
  "structured binding in condition requires explicit 'auto' type",
  "条件中的结构化绑定需要显式 'auto' 类型")
```

#### 实施检查清单

- [ ] `include/blocktype/AST/Decl.h` 中已添加 BindingDecl 类
- [ ] `NodeKinds.def` 中已添加 BindingDecl
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnDecompositionDecl
- [ ] Parser 支持 `auto [a, b] = expr` 和 `if (auto [a, b] = expr)`
- [ ] CodeGen 支持 BindingDecl 的代码生成

---

### Task 7.4.4 Pack Indexing 完善 (P2662R3)

**状态：** 已部分实现，需验证和完善。

#### 现有接口验证清单

**文件：** `include/blocktype/AST/Expr.h`

已有 `PackIndexingExpr`（确认存在）。需验证：

```cpp
// PackIndexingExpr 需要的完整接口：
class PackIndexingExpr : public Expr {
  Expr *Pack;           // 被索引的包
  Expr *Index;          // 索引表达式
  // ⚠️ 需要添加的字段：
  llvm::SmallVector<Expr *, 4> SubstitutedExprs;  // 实例化后的具体表达式

public:
  Expr *getPack() const;
  Expr *getIndex() const;
  llvm::ArrayRef<Expr *> getSubstitutedExprs() const;
  void setSubstitutedExprs(llvm::ArrayRef<Expr *> Exprs);

  /// 是否已完成包展开替换
  bool isSubstituted() const { return !SubstitutedExprs.empty(); }
};
```

#### 实施检查清单

- [ ] PackIndexingExpr 有 SubstitutedExprs 字段
- [ ] Sema 正确处理模板实例化中的包索引
- [ ] CodeGen 正确生成具体化后的代码
- [ ] 新增 5+ 测试用例（包括多模板参数、嵌套包等）

---

## Stage 7.5 — 其他特性 + 测试（详细接口）

### Task 7.5.1 转义序列扩展（详细接口）

> **背景：** P2290R3 引入分隔转义 `\x{...}`，P2071R2 引入命名转义 `\N{...}`。当前 `Lexer::processEscapeSequence()` 仅支持 `\xHH`、`\uXXXX`、`\UXXXXXXXX`。需要在 `Lexer` 中新增两个解析方法并集成 Unicode 字符名数据库。

#### E7.5.1.1 Lexer.h 接口修改

**修改文件：** `include/blocktype/Lex/Lexer.h`

```cpp
class Lexer {
  // ... 现有声明 ...

private:
  // === ⚠️ P7.5.1 新增方法 ===

  /// 解析分隔转义 \x{HHHH...} (P2290R3)
  /// 前置条件：BufferPtr 指向 '{' 之后第一个字符
  /// 返回 true 如果成功解析，false 如果遇到错误
  ///
  /// **Clang 参考**：
  /// - `clang/lib/Lex/LiteralSupport.cpp` EscapeSpecifier::ProcessEscapeSequence()
  bool processDelimitedHexEscape();

  /// 解析命名转义 \N{name} (P2071R2)
  /// 前置条件：BufferPtr 指向 '{' 之后第一个字符
  /// 返回 true 如果成功解析，false 如果遇到错误
  ///
  /// **Clang 参考**：
  /// - `clang/lib/Lex/LiteralSupport.cpp` 命名转义处理
  bool processNamedEscape();

  /// 查找 Unicode 字符名 → 码点映射
  /// \param Name 字符名（如 "LATIN CAPITAL LETTER A"）
  /// \param CodePoint [out] 找到的 Unicode 码点
  /// \return true 如果找到，false 如果名称未知
  static bool lookupUnicodeName(llvm::StringRef Name, uint32_t &CodePoint);
};
```

#### E7.5.1.2 processEscapeSequence() 修改

**修改文件：** `src/Lex/Lexer.cpp`

```cpp
bool Lexer::processEscapeSequence() {
  if (BufferPtr >= BufferEnd) return false;
  char C = *BufferPtr;

  // ... 现有的 \e、\xHH、\OOO 处理保持不变 ...

  // ⚠️ P2290R3: 在 \x 分支内增加分隔转义检测
  if (C == 'x') {
    ++BufferPtr;
    // 新增：检测 \x{...} 分隔转义
    if (BufferPtr < BufferEnd && *BufferPtr == '{') {
      return processDelimitedHexEscape();
    }
    // 原有 \xHH 逻辑...
  }

  // ⚠️ P2071R2: 新增 \N{...} 命名转义（在 \u/\U 之后）
  if (C == 'N') {
    ++BufferPtr;
    if (BufferPtr < BufferEnd && *BufferPtr == '{') {
      return processNamedEscape();
    }
    Diags.report(getSourceLocation(), DiagLevel::Error,
                 "expected '{' after '\\N' for named escape");
    return false;
  }

  // ... 现有 \u、\U、简单转义保持不变 ...
}
```

#### E7.5.1.3 Unicode 字符名数据库

**新增文件：** `third_party/unicode_names.inc`

```cpp
// 由 scripts/generate_unicode_names.py 从 UnicodeData.txt 生成
// 包含约 34,000 个 Unicode 字符名映射
//
// 格式：{ "CHARACTER_NAME", 0xXXXX }
// 表按名称排序以支持二分查找

struct UnicodeNameEntry {
  const char *Name;
  uint32_t CodePoint;
};

static const UnicodeNameEntry UnicodeNameTable[] = {
#define UNICODE_NAME(NAME, CP) { NAME, CP },
#include "unicode_names.inc"
#undef UNICODE_NAME
};
```

**新增脚本：** `scripts/generate_unicode_names.py`

- 输入：`UnicodeData.txt`（Unicode Character Database）、`NameAliases.txt`
- 输出：`third_party/unicode_names.inc`
- 每行格式：`UNICODE_NAME("LATIN CAPITAL LETTER A", 0x0041)`
- 支持名称别名

#### E7.5.1.4 诊断 ID

**修改文件：** `include/blocktype/Basic/DiagnosticLexKinds.def`

```cpp
// === P7.5.1 转义序列扩展诊断 ===

// P2290R3 分隔转义
DIAG(err_delimited_escape_empty, Error,
  "delimited escape sequence must have at least one hex digit",
  "分隔转义序列必须包含至少一个十六进制数字")

DIAG(err_delimited_escape_unterminated, Error,
  "expected '}' to end delimited escape sequence",
  "分隔转义序列缺少结束的 '}'")

DIAG(err_delimited_escape_out_of_range, Error,
  "hex escape sequence out of Unicode range",
  "十六进制转义序列超出 Unicode 范围")

// P2071R2 命名转义
DIAG(err_named_escape_expected_brace, Error,
  "expected '{' after '\\N'",
  "'\\N' 后需要 '{'")

DIAG(err_named_escape_unterminated, Error,
  "expected '}' to end named escape sequence",
  "命名转义序列缺少结束的 '}'")

DIAG(err_named_escape_unknown_name, Error,
  "unknown Unicode character name",
  "未知的 Unicode 字符名")
```

#### E7.5.1.5 实现步骤

1. **Step 1**: 生成 `unicode_names.inc` 并放置到 `third_party/`
2. **Step 2**: 在 `Lexer.h` 中声明 3 个新方法
3. **Step 3**: 在 `DiagnosticLexKinds.def` 中添加 6 个诊断 ID
4. **Step 4**: 实现 `processDelimitedHexEscape()`
5. **Step 5**: 实现 `processNamedEscape()` 和 `lookupUnicodeName()`
6. **Step 6**: 修改 `processEscapeSequence()` 添加新分支
7. **Step 7**: 编写测试用例（至少 5 个）

---

### Task 7.5.2 其他 C++26 特性（详细接口）

> **背景：** 这些特性各有独立的接口需求。按照"接口先行"原则，逐一列出。

#### E7.5.2.1 `for` init-statement 中 `using` (P2360R0)

**修改文件：** `src/Parse/ParseStmt.cpp`

```cpp
// 在 parseForStatement 中，允许 init-statement 解析 using 声明
// 语法：for (using namespace std; cond; inc) { body }
//
// 关键修改：parseForStatement 中的 init-statement 解析逻辑
// 当前：只允许表达式语句和声明语句
// 修改后：也允许 UsingDecl / UsingDirectiveDecl
//
// **Clang 参考**：
// - `clang/lib/Parse/ParseStmt.cpp` 中 `ParseForStatement()`
// - init-statement 直接调用 `ParseSimpleDeclaration()` 即可
//   因为 `using` 本身就是声明的一种
```

**修改文件：** `include/blocktype/AST/Stmt.h`

```cpp
// ForStmt 的 Init 字段已经是 Stmt*，可以包裹 DeclStmt
// DeclStmt 中已经可以包含 UsingDecl
// 无需新增 AST 节点，只需 Parser 允许解析即可
```

**诊断ID：**

```cpp
DIAG(err_using_in_for_init_not_supported, Error,
  "'using' declaration in for-loop initialization is a C++23 extension",
  "for 循环初始化中的 'using' 声明是 C++23 扩展")
```

---

#### E7.5.2.2 `@`/`$`/反引号字符集扩展 (P2558R2)

**修改文件：** `src/Lex/TokenKinds.cpp`

```cpp
// 已有：@ token（用于预处理器等）
// 需新增：$ 作为标识符字符
//         反引号 `` 作为标识符分隔符
```

**修改文件：** `include/blocktype/Lex/Lexer.h`

```cpp
class Lexer {
public:
  // ⚠️ P7.5.2.2 新增

  /// 检查字符是否可作为扩展标识符字符（$, @, 反引号）
  bool isExtendedIdentifierChar(uint32_t CP) const;

  /// 检查是否为扩展标识符起始字符
  bool isExtendedIdentifierStart(uint32_t CP) const;
};
```

**修改文件：** `include/blocktype/Lex/TokenKinds.def`

```cpp
// 新增 token 类型
// DOLLAR     — $ 字符（作为标识符一部分或独立 token）
// BACKTICK   — ` 反引号（标识符分隔符）
```

**诊断ID：**

```cpp
DIAG(ext_dollar_in_identifier, Extension,
  "'$' in identifier is a BlockType extension",
  "标识符中的 '$' 是 BlockType 扩展")

DIAG(warn_backtick_identifier, Warning,
  "backtick-delimited identifiers are a BlockType extension",
  "反引号分隔的标识符是 BlockType 扩展")
```

---

#### E7.5.2.3 `[[indeterminate]]` 属性 (P2795R5)

**修改文件：** `include/blocktype/AST/Decl.h`

```cpp
/// IndeterminateAttr — [[indeterminate]] 属性
///
/// **语义**：标记变量为"值不确定"状态，编译器不需要初始化
/// **用法**：int x [[indeterminate]];
///
/// **Clang 参考**：
/// - `clang/include/clang/Basic/Attr.td` 中 `IndeterminateAttr`
/// - CodeGen 中跳过变量初始化
class IndeterminateAttr {
public:
  static constexpr llvm::StringRef getAttrName() { return "indeterminate"; }
};
```

**修改文件：** `src/CodeGen/CodeGenStmt.cpp`

```cpp
// 在 EmitVarDecl 中：
// if (VD->hasAttr("indeterminate")) {
//   // 跳过零初始化，只创建 alloca
//   auto *Alloca = CreateAlloca(Ty, VD->getName());
//   setLocalDecl(VD, Alloca);
//   return; // 不初始化
// }
```

**诊断ID：**

```cpp
DIAG(warn_indeterminate_not_scalar, Warning,
  "'[[indeterminate]]' attribute ignored for non-scalar type '%0'",
  "'[[indeterminate]]' 属性对非标量类型 '%0' 被忽略")

DIAG(err_indeterminate_with_init, Error,
  "variable with '[[indeterminate]]' attribute cannot have an initializer",
  "带有 '[[indeterminate]]' 属性的变量不能有初始化器")
```

---

#### E7.5.2.4 平凡可重定位 (P2786R13)

**修改文件：** `include/blocktype/Sema/Sema.h`

```cpp
class Sema {
public:
  // ⚠️ P7.5.2.4 新增

  /// 检查类型是否是平凡可重定位的
  ///
  /// **规则**：
  /// - 标量类型：是
  /// - 仅包含平凡可重定位成员的类：是
  /// - 有用户定义的移动/拷贝操作：需要逐个判断
  ///
  /// **Clang 参考**：
  /// - `clang/lib/AST/Type.cpp` 中 `isTriviallyRelocatable()`
  bool isTriviallyRelocatable(QualType T) const;
};
```

**修改文件：** `include/blocktype/AST/Type.h`

```cpp
class Type {
public:
  // ⚠️ P7.5.2.4 新增

  /// 检查类型是否是平凡可重定位的
  bool isTriviallyRelocatable() const;
};
```

---

#### E7.5.2.5 概念和变量模板模板参数 (P2841R7)

**修改文件：** `include/blocktype/AST/Decl.h`

```cpp
// TemplateTemplateParmDecl 已存在，需要扩展支持概念约束

class TemplateTemplateParmDecl : public TemplateDecl {
  // ... 现有字段 ...

  // ⚠️ P7.5.2.5 新增
  Expr *RequiresClause = nullptr;  // 概念约束
  bool IsConstrained = false;

public:
  bool isConstrained() const { return IsConstrained; }
  Expr *getRequiresClause() const { return RequiresClause; }
  void setRequiresClause(Expr *E) { RequiresClause = E; IsConstrained = (E != nullptr); }
};
```

**修改文件：** `include/blocktype/Sema/Sema.h`

```cpp
class Sema {
public:
  // ⚠️ P7.5.2.5 新增

  /// 检查模板模板参数是否满足概念约束
  bool CheckTemplateTemplateParameterConstraint(
      TemplateDecl *Template, ConceptDecl *Constraint, SourceLocation Loc);
};
```

---

#### E7.5.2.6 可变参数友元 (P2893R3)

**修改文件：** `src/Parse/ParseClass.cpp`

```cpp
// 在 parseFriendDeclaration 中允许参数包展开
// 语法：template <typename... Ts> friend class Ts...;
//
// **Clang 参考**：
// - `clang/lib/Parse/ParseDeclCXX.cpp` 中 `Parser::ParseFriendDeclaration()`
// - FriendDecl 需要支持 pack expansion
```

**修改文件：** `include/blocktype/AST/Decl.h`

```cpp
class FriendDecl : public Decl {
  // ... 现有字段 ...

  // ⚠️ P7.5.2.6 新增
  bool IsPackExpansion = false;  // variadic friend

public:
  bool isPackExpansion() const { return IsPackExpansion; }
  void setPackExpansion(bool V) { IsPackExpansion = V; }
};
```

---

#### E7.5.2.7 `constexpr` 异常 (P3068R6)

**修改文件：** `src/Parse/ParseStmt.cpp`

```cpp
// 在 constexpr 函数中允许 throw 表达式
// 当前限制：throw 在 constexpr 函数中报错
// 修改后：constexpr 函数中的 throw 在编译期求值时
//         如果执行到则编译失败，否则正常
//
// **关键修改**：
// - Sema 中移除 "throw in constexpr function" 硬错误
// - 改为在 ConstantExpr 求值时检查
```

**修改文件：** `src/Sema/Sema.cpp`

```cpp
// 在 ActOnThrowExpr 或相关检查中：
// 旧：if (CurFD->isConstexpr()) emitError("...");
// 新：if (CurFD->isConstexpr()) emitWarning("throw in constexpr function");
//     // 允许但运行时检查
```

---

#### E7.5.2.8 可观察检查点 (P1494R5)

**修改文件：** `src/CodeGen/CodeGenFunction.cpp`

```cpp
// 实现编译器内置宏或属性，标记可观察行为检查点
// 语法：__builtin_observer_checkpoint() 或 [[observable_checkpoint]]
//
// **Clang 参考**：
// - `clang/include/clang/Basic/Builtins.def` 中 `__builtin_is_constant_evaluated`
// - 类似方式添加 `__builtin_observer_checkpoint`
```

**修改文件：** `include/blocktype/Lex/TokenKinds.def`

```cpp
// 新增关键字（如果使用属性语法则不需要新关键字）
// kw_observable_checkpoint（如果使用关键字语法）
```

---

## 📋 NodeKinds.def 完整清单（现有 vs 需新增）

> **背景：** `NodeKinds.def` 定义了所有 AST 节点类型。Phase 7 需要新增以下节点。

### 现有节点（NodeKinds.def 当前内容）

```
// === Expression Nodes（39 个）===
ABSTRACT_EXPR(Expr, ASTNode)
EXPR(IntegerLiteral, Expr)            EXPR(FloatingLiteral, Expr)
EXPR(StringLiteral, Expr)             EXPR(CharacterLiteral, Expr)
EXPR(CXXBoolLiteral, Expr)            EXPR(CXXNullPtrLiteral, Expr)
EXPR(DeclRefExpr, Expr)               EXPR(TemplateSpecializationExpr, Expr)
EXPR(MemberExpr, Expr)                EXPR(ArraySubscriptExpr, Expr)
EXPR(BinaryOperator, Expr)            EXPR(UnaryOperator, Expr)
EXPR(UnaryExprOrTypeTraitExpr, Expr)  EXPR(ConditionalOperator, Expr)
EXPR(CallExpr, Expr)                  EXPR(CXXMemberCallExpr, CallExpr)
EXPR(CXXConstructExpr, Expr)          EXPR(CXXTemporaryObjectExpr, CXXConstructExpr)
EXPR(CXXThisExpr, Expr)               EXPR(CXXNewExpr, Expr)
EXPR(CXXDeleteExpr, Expr)             EXPR(CXXThrowExpr, Expr)
ABSTRACT_EXPR(CastExpr, Expr)
EXPR(CXXStaticCastExpr, CastExpr)     EXPR(CXXDynamicCastExpr, CastExpr)
EXPR(CXXConstCastExpr, CastExpr)      EXPR(CXXReinterpretCastExpr, CastExpr)
EXPR(CStyleCastExpr, CastExpr)
EXPR(InitListExpr, Expr)              EXPR(DesignatedInitExpr, Expr)
EXPR(LambdaExpr, Expr)
EXPR(RequiresExpr, Expr)              EXPR(CXXFoldExpr, Expr)
EXPR(PackIndexingExpr, Expr)          EXPR(ReflexprExpr, Expr)
EXPR(CoawaitExpr, Expr)
EXPR(CXXDependentScopeMemberExpr, Expr)
EXPR(DependentScopeDeclRefExpr, Expr)
EXPR(RecoveryExpr, Expr)

// === Statement Nodes（22 个）===
ABSTRACT_STMT(Stmt, ASTNode)
STMT(NullStmt, Stmt)     STMT(CompoundStmt, Stmt)    STMT(ReturnStmt, Stmt)
STMT(ExprStmt, Stmt)     STMT(DeclStmt, Stmt)
STMT(IfStmt, Stmt)       STMT(SwitchStmt, Stmt)      STMT(CaseStmt, Stmt)
STMT(DefaultStmt, Stmt)  STMT(BreakStmt, Stmt)       STMT(ContinueStmt, Stmt)
STMT(GotoStmt, Stmt)     STMT(LabelStmt, Stmt)
STMT(WhileStmt, Stmt)    STMT(DoStmt, Stmt)           STMT(ForStmt, Stmt)
STMT(CXXForRangeStmt, Stmt)
STMT(CXXTryStmt, Stmt)   STMT(CXXCatchStmt, Stmt)
STMT(CoreturnStmt, Stmt) STMT(CoyieldStmt, Stmt)

// === Declaration Nodes（37 个）===
ABSTRACT_DECL(Decl, ASTNode)
DECL(NamedDecl, Decl)    DECL(ValueDecl, NamedDecl)
DECL(VarDecl, ValueDecl) DECL(ParmVarDecl, VarDecl)
DECL(FunctionDecl, ValueDecl)
DECL(FieldDecl, ValueDecl)    DECL(EnumConstantDecl, ValueDecl)
DECL(LabelDecl, NamedDecl)    DECL(TypeDecl, NamedDecl)
DECL(TypedefDecl, TypeDecl)   DECL(TagDecl, TypeDecl)
DECL(EnumDecl, TagDecl)       DECL(RecordDecl, TagDecl)
DECL(CXXRecordDecl, RecordDecl)
DECL(CXXMethodDecl, FunctionDecl)
DECL(CXXConstructorDecl, CXXMethodDecl)
DECL(CXXDestructorDecl, CXXMethodDecl)
DECL(CXXConversionDecl, CXXMethodDecl)
DECL(AccessSpecDecl, Decl)    DECL(NamespaceDecl, NamedDecl)
DECL(TranslationUnitDecl, Decl)
DECL(UsingDecl, NamedDecl)    DECL(UsingDirectiveDecl, NamedDecl)
DECL(NamespaceAliasDecl, NamedDecl) DECL(UsingEnumDecl, Decl)
DECL(TemplateDecl, NamedDecl)
DECL(FunctionTemplateDecl, TemplateDecl)
DECL(ClassTemplateDecl, TemplateDecl)
DECL(VarTemplateDecl, TemplateDecl)
DECL(TypeAliasTemplateDecl, TemplateDecl)
DECL(TemplateTemplateParmDecl, TemplateDecl)
DECL(TemplateTypeParmDecl, TypeDecl)
DECL(NonTypeTemplateParmDecl, ValueDecl)
DECL(ClassTemplateSpecializationDecl, CXXRecordDecl)
DECL(ClassTemplatePartialSpecializationDecl, ClassTemplateSpecializationDecl)
DECL(VarTemplateSpecializationDecl, VarDecl)
DECL(VarTemplatePartialSpecializationDecl, VarTemplateSpecializationDecl)
DECL(ModuleDecl, NamedDecl)   DECL(ImportDecl, NamedDecl)
DECL(ExportDecl, Decl)        DECL(StaticAssertDecl, Decl)
DECL(LinkageSpecDecl, Decl)   DECL(TypeAliasDecl, TypeDecl)
DECL(FriendDecl, Decl)        DECL(ConceptDecl, TypeDecl)
DECL(AsmDecl, Decl)           DECL(CXXDeductionGuideDecl, FunctionDecl)
DECL(AttributeDecl, Decl)     DECL(AttributeListDecl, Decl)
```

### Phase 7 需要新增的节点

```cpp
// === NodeKinds.def Phase 7 新增 ===

// Task 7.1.2: decay-copy 表达式
EXPR(DecayCopyExpr, Expr)

// Task 7.2.1: reflexpr 已存在（ReflexprExpr）

// Task 7.3.1: Contract 属性（作为 Decl 而非 Expr）
// Contract 作为 AttributeDecl 的子类
DECL(ContractDecl, Decl)

// Task 7.4.2: 占位符变量（VarDecl 扩展，无需新节点）

// Task 7.4.3: 结构化绑定
DECL(BindingDecl, ValueDecl)

// Task 7.5.4: 多维下标（ArraySubscriptExpr 已支持多参数，无需新节点）
// 注意：当前 ArraySubscriptExpr 已经通过 SmallVector 支持多个索引
// 如果需要区分，可以新增：
// EXPR(MultiDimensionalSubscriptExpr, Expr)

// Task 7.5.6: Z/z 字面量后缀（IntegerLiteral 扩展，无需新节点）
```

---

## 📋 TypeNodes.def 完整清单（现有 vs 需新增）

### 现有 TypeClass 值

```cpp
// TypeNodes.def 当前内容
TYPE(Builtin, Type)           // BuiltinType
TYPE(Pointer, Type)           // PointerType
TYPE(LValueReference, Type)   // LValueReferenceType
TYPE(RValueReference, Type)   // RValueReferenceType
TYPE(ConstantArray, Type)     // ConstantArrayType
TYPE(IncompleteArray, Type)   // IncompleteArrayType
TYPE(VariableArray, Type)     // VariableArrayType
TYPE(Function, Type)          // FunctionType
TYPE(Record, Type)            // RecordType
TYPE(Enum, Type)              // EnumType
TYPE(Typedef, Type)           // TypedefType
TYPE(Elaborated, Type)        // ElaboratedType
TYPE(Unresolved, Type)        // UnresolvedType
TYPE(MemberPointer, Type)     // MemberPointerType
TYPE(TemplateTypeParm, Type)  // TemplateTypeParmType
TYPE(Dependent, Type)         // DependentType
TYPE(TemplateSpecialization, Type) // TemplateSpecializationType
TYPE(Auto, Type)              // AutoType
TYPE(Decltype, Type)          // DecltypeType
```

### Phase 7 需要新增

```cpp
// Task 7.2.1: 反射信息类型
TYPE(MetaInfo, Type)          // meta::info 反射类型
```

---

## 📋 Attr.h 完整定义（Phase 7 需要新建）

> **背景：** 当前代码库使用 `AttributeDecl` 和 `AttributeListDecl` 存储属性信息。
> Phase 7 需要更丰富的属性类型系统。新文件 `include/blocktype/AST/Attr.h`
> 定义具体的属性类型。

**文件：** `include/blocktype/AST/Attr.h`（新建）

```cpp
#pragma once
#include "blocktype/AST/ASTNode.h"
#include "blocktype/AST/Expr.h"

namespace blocktype {

/// AttrKind — 属性种类枚举
enum class AttrKind {
  // C++11 标准属性
  Assume,           // [[assume(expr)]] (P1774R8)
  CarriesDependency,// [[carries_dependency]]
  Deprecated,       // [[deprecated]] / [[deprecated("msg")]]
  Fallthrough,      // [[fallthrough]]
  Likely,           // [[likely]]
  Unlikely,         // [[unlikely]]
  MaybeUnused,      // [[maybe_unused]]
  Nodiscard,        // [[nodiscard]] / [[nodiscard("msg")]]
  Noreturn,         // [[noreturn]]
  NoUniqueAddress,  // [[no_unique_address]]

  // C++26 扩展属性
  Indeterminate,    // [[indeterminate]] (P2795R5)

  // Contracts (P2900R14)
  ContractPre,      // [[pre: expr]]
  ContractPost,     // [[post: expr]]
  ContractAssert,   // [[assert: expr]]
};

/// Attr — 所有属性的基类
class Attr {
  AttrKind Kind;
  SourceLocation Loc;

public:
  Attr(AttrKind K, SourceLocation L) : Kind(K), Loc(L) {}
  virtual ~Attr() = default;

  AttrKind getKind() const { return Kind; }
  SourceLocation getLocation() const { return Loc; }

  virtual void dump(llvm::raw_ostream &OS) const;
};

/// AssumeAttr — [[assume(condition)]] (P1774R8)
///
/// **语义**：告诉编译器 condition 在此点一定为 true。
/// 编译器可据此进行优化（生成 llvm.assume intrinsic）。
///
/// **Clang 参考**：
/// - `clang/include/clang/AST/Attr.h` 中 `AssumeAttr`
/// - `clang/lib/CodeGen/CodeGenFunction.cpp` 中 `EmitAssumeAttr()`
class AssumeAttr : public Attr {
  Expr *Condition;

public:
  AssumeAttr(SourceLocation Loc, Expr *Cond)
      : Attr(AttrKind::Assume, Loc), Condition(Cond) {}

  Expr *getCondition() const { return Condition; }

  void dump(llvm::raw_ostream &OS) const override;

  static bool classof(const Attr *A) { return A->getKind() == AttrKind::Assume; }
};

/// ContractAttr — Contracts 属性 (P2900R14)
///
/// **语法**：
///   [[pre: condition]]    — 前置条件
///   [[post: condition]]   — 后置条件（支持 result 变量）
///   [[assert: condition]] — 断言
///
/// **Clang 参考**：
/// - `clang/include/clang/AST/Attr.h` 中 `ContractAttr`
/// - `clang/lib/Sema/SemaContract.cpp`
class ContractAttr : public Attr {
  Expr *Condition;
  bool IsPre;
  bool IsPost;
  bool IsAssert;
  llvm::StringRef Comment;  // 可选注释

public:
  /// 创建前置条件
  static ContractAttr *CreatePre(SourceLocation Loc, Expr *Cond,
                                  llvm::StringRef Comment = "") {
    return new ContractAttr(Loc, Cond, true, false, false, Comment);
  }

  /// 创建后置条件
  static ContractAttr *CreatePost(SourceLocation Loc, Expr *Cond,
                                   llvm::StringRef Comment = "") {
    return new ContractAttr(Loc, Cond, false, true, false, Comment);
  }

  /// 创建断言
  static ContractAttr *CreateAssert(SourceLocation Loc, Expr *Cond,
                                     llvm::StringRef Comment = "") {
    return new ContractAttr(Loc, Cond, false, false, true, Comment);
  }

  bool isPre() const { return IsPre; }
  bool isPost() const { return IsPost; }
  bool isAssert() const { return IsAssert; }
  Expr *getCondition() const { return Condition; }
  llvm::StringRef getComment() const { return Comment; }

  void dump(llvm::raw_ostream &OS) const override;

  static bool classof(const Attr *A) {
    return A->getKind() == AttrKind::ContractPre ||
           A->getKind() == AttrKind::ContractPost ||
           A->getKind() == AttrKind::ContractAssert;
  }

private:
  ContractAttr(SourceLocation Loc, Expr *Cond,
               bool Pre, bool Post, bool Assert, llvm::StringRef Comment)
      : Attr(Pre ? AttrKind::ContractPre :
               Post ? AttrKind::ContractPost : AttrKind::ContractAssert,
             Loc),
        Condition(Cond), IsPre(Pre), IsPost(Post), IsAssert(Assert),
        Comment(Comment) {}
};

/// IndeterminateAttr — [[indeterminate]] (P2795R5)
///
/// **语义**：标记变量为值不确定状态，编译器不进行零初始化。
class IndeterminateAttr : public Attr {
public:
  IndeterminateAttr(SourceLocation Loc)
      : Attr(AttrKind::Indeterminate, Loc) {}

  void dump(llvm::raw_ostream &OS) const override;

  static bool classof(const Attr *A) {
    return A->getKind() == AttrKind::Indeterminate;
  }
};

} // namespace blocktype
```

---

## 📊 完整文件清单

### 需要新建的文件

| 文件 | 服务于 | 优先级 |
|------|--------|--------|
| `include/blocktype/AST/Attr.h` | 7.1.4, 7.3.1, 7.5.2.3 | P0 |
| `include/blocktype/Sema/SemaCXX.h` | 7.1.1, 7.1.3, 7.3 | P0 |
| `include/blocktype/Sema/TypeCheck.h` | 7.1.2 | P0 |
| `include/blocktype/Sema/SemaReflection.h` | 7.2.1, 7.2.2 | P1 |
| `include/blocktype/AST/ReflectionTypes.h` | 7.2.1 | P1 |
| `include/blocktype/CodeGen/CGAttrs.h` | 7.1.4 | P1 |
| `src/Sema/SemaCXX.cpp` | 7.1.1, 7.1.3, 7.3 | P0 |
| `src/Sema/TypeCheck.cpp` | 7.1.2 | P0 |
| `src/Sema/SemaReflection.cpp` | 7.2.1, 7.2.2 | P1 |
| `src/CodeGen/CGAttrs.cpp` | 7.1.4 | P1 |
| `src/AST/Attr.cpp` | 7.1.4, 7.3.1 | P1 |
| `tests/cpp23/*.cpp` | 7.1 | P0 |
| `tests/cpp26/*.cpp` | 7.4 | P1 |

### 需要修改的文件

| 文件 | 修改内容 | Task |
|------|---------|------|
| `include/blocktype/AST/Decl.h` | FunctionDecl/CXXMethodDecl/VarDecl/BindingDecl 扩展 | 7.1.1, 7.1.3, 7.4.1, 7.4.2, 7.4.3 |
| `include/blocktype/AST/Expr.h` | 新增 DecayCopyExpr, ReflexprExpr | 7.1.2, 7.2.1 |
| `include/blocktype/AST/Attr.h` | 新增 AssumeAttr, ContractAttr, IndeterminateAttr | 7.1.4, 7.3.1, 7.5.2.3 |
| `include/blocktype/AST/Type.h` | TypeClass 枚举扩展 (+MetaInfo) | 7.2.1 |
| `include/blocktype/AST/NodeKinds.def` | 新增 DecayCopyExpr, BindingDecl, ContractDecl | 全部 |
| `include/blocktype/AST/TypeNodes.def` | 新增 MetaInfo | 7.2.1 |
| `include/blocktype/CodeGen/CGCXX.h` | 新增方法声明 | 7.1.1 |
| `include/blocktype/CodeGen/CodeGenFunction.h` | 新增属性相关 Emit 方法 | 7.1.4, 7.3.1 |
| `include/blocktype/Sema/Sema.h` | 新增 ActOn* 方法 | 7.1.2, 7.4.2, 7.4.3, 7.5.2.4 |
| `include/blocktype/Lex/TokenKinds.def` | 新增 token（$、反引号等） | 7.5.2.2 |
| `include/blocktype/Lex/Lexer.h` | 扩展标识符支持 | 7.5.2.2 |
| `include/blocktype/Lex/Preprocessor.h` | DirectiveKind + Warning | 7.5.5 |
| `include/blocktype/Basic/DiagnosticSemaKinds.def` | 新增诊断ID（约30+条） | 全部 |
| `src/Parse/ParseDecl.cpp` | 解析 deducing this, delete("reason"), 占位符, binding | 7.1.1, 7.4.1, 7.4.2, 7.4.3 |
| `src/Parse/ParseExpr.cpp` | 解析 decay-copy, 多维下标, Z/z 后缀 | 7.1.2, 7.5.4, 7.5.6 |
| `src/Parse/ParseStmt.cpp` | for-using, 结构化绑定条件 | 7.5.2.1, 7.4.3 |
| `src/Parse/ParseClass.cpp` | static operator, variadic friend | 7.1.3, 7.5.2.6 |
| `src/Sema/Sema.cpp` | 集成所有检查逻辑 | 全部 |
| `src/CodeGen/CGCXX.cpp` | CodeGen deducing this, static operator | 7.1.1, 7.1.3 |
| `src/CodeGen/CodeGenStmt.cpp` | 占位符变量, indeterminate 属性 | 7.4.2, 7.5.2.3 |
| `src/Lex/Lexer.cpp` | 分隔转义, 命名转义, Z/z 后缀 | 7.5.1, 7.5.6 |
| `src/Lex/Preprocessor.cpp` | #warning 指令 | 7.5.5 |

---

## ⚠️ 实施检查清单

在开始每个Task之前，必须确认：

### Task 7.1.1 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 FunctionDecl 已添加 ExplicitObjectParam 和 HasExplicitObjectParam 字段
- [ ] `include/blocktype/AST/Decl.h` 中 FunctionDecl 已添加 getThisType() 方法声明
- [ ] `include/blocktype/Sema/SemaCXX.h` 已创建并声明 CheckExplicitObjectParameter 和 DeduceExplicitObjectType
- [ ] `include/blocktype/CodeGen/CGCXX.h` 已添加 EmitExplicitObjectParameterCall 和 AdjustObjectForExplicitParam
- [ ] `include/blocktype/Basic/DiagnosticSemaKinds.def` 已添加 5 条相关诊断ID

### Task 7.1.2 开始前
- [ ] `include/blocktype/AST/Expr.h` 中已添加 DecayCopyExpr 类（含 getSubExpr, isDirectInit, classof）
- [ ] `NodeKinds.def` 已添加 EXPR(DecayCopyExpr, Expr)
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnDecayCopyExpr
- [ ] `include/blocktype/Sema/TypeCheck.h` 已创建并声明 Decay/CanCopyInitialize/CanDirectInitialize
- [ ] `include/blocktype/Basic/DiagnosticSemaKinds.def` 已添加 decay-copy 相关诊断ID

### Task 7.1.3 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 CXXMethodDecl 已添加 IsStaticOperator 字段
- [ ] `include/blocktype/Sema/SemaCXX.h` 中已声明 CheckStaticOperator

### Task 7.1.4 开始前
- [ ] `include/blocktype/AST/Attr.h` 已创建（含 AttrKind 枚举、Attr 基类、AssumeAttr）
- [ ] `include/blocktype/CodeGen/CGAttrs.h` 已创建并声明 EmitAssumeAttr

### Task 7.2.1 开始前
- [ ] `include/blocktype/AST/Expr.h` 中已添加 ReflexprExpr 类（含 OK_Type/OK_Expression 判别）
- [ ] `include/blocktype/AST/ReflectionTypes.h` 已创建（含 meta::InfoType 类）
- [ ] `TypeNodes.def` 已添加 TYPE(MetaInfo, Type)
- [ ] `include/blocktype/Sema/SemaReflection.h` 已创建（含反射语义分析方法）

### Task 7.3.1 开始前
- [x] `include/blocktype/AST/Attr.h` 中已添加 ContractAttr（含 BuildContractAttr 工厂方法）
- [x] `include/blocktype/Sema/SemaCXX.h` 中已声明 CheckContractCondition / CheckContractPlacement / BuildContractAttr

### Task 7.4.1 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 FunctionDecl 已添加 DeletedReason（StringLiteral*）字段
- [ ] `include/blocktype/Basic/DiagnosticSemaKinds.def` 已添加 err_use_of_deleted_function_with_reason

### Task 7.4.2 开始前
- [ ] `include/blocktype/AST/Decl.h` 中 VarDecl 已添加 IsPlaceholder 字段
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnPlaceholderVarDecl 和 isPlaceholderIdentifier

### Task 7.4.3 开始前
- [ ] `NodeKinds.def` 已添加 DECL(BindingDecl, ValueDecl)
- [ ] `include/blocktype/AST/Decl.h` 中已添加 BindingDecl 类（含 IsPackExpansion）
- [ ] `include/blocktype/Sema/Sema.h` 中已声明 ActOnDecompositionDecl

### Task 7.5.1 开始前
- [ ] `include/blocktype/Lex/Lexer.h` 中已添加分隔转义/命名转义解析方法声明

### Task 7.5.2 开始前
- [ ] 确认 `include/blocktype/AST/Attr.h` 中有 IndeterminateAttr
- [ ] 确认 `include/blocktype/Lex/TokenKinds.def` 中有 $ 和反引号 token
- [ ] 确认 FriendDecl 已添加 IsPackExpansion 字段

---

## ~~🆕 新增：核查发现的未实现特性（2026-04-19）~~ → ✅ 已全部确认实现

> **背景：** 在核查Phase 7特性状态时，初次搜索误判 3 项"已实现"特性为未实现。
> **补充核查（2026-04-19）：** 经精确搜索确认，这 3 项特性均已实现，以下 Task 可取消。
>
> - **多维 `operator[]`** — 实际实现：`ParseExpr.cpp:450-460`
> - **`#warning`** — 实际实现：`Preprocessor.cpp:380-381, 1510-1515`
> - **`Z`/`z` 后缀** — 实际实现：`ParseExpr.cpp:704-706`

### ~~Task 7.5.4 多维 `operator[]` (P2128R6)~~ ✅ 已确认实现

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

### ~~Task 7.5.5 `#warning` 预处理指令 (P2437R1)~~ ✅ 已确认实现

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

### ~~Task 7.5.6 `Z`/`z` 字面量后缀 (P0330R8)~~ ✅ 已确认实现

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
- BlockType 现有架构：`docs/architecture/ARCHITECTURE.md`

**下一步行动：**
1. 按本文件清单逐个创建/修改头文件
2. 确保编译通过（即使实现是空壳）
3. 再开始实现 .cpp 文件
4. 最后编写测试

---

*文档维护者：AI Assistant*
*最后更新：2026-04-19*
