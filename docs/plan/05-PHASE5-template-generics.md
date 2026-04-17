# Phase 5：模板与泛型
> **目标：** 实现完整的模板系统，包括类模板、函数模板、模板参数推导、Concepts、变参模板等
> **前置依赖：** Phase 0-4 完成（语义分析基础）
> **验收标准：** 能正确编译和实例化标准库级别的模板代码

---

## 📌 阶段总览

```
Phase 5 包含 5 个 Stage，共 15 个 Task，预计 6 周完成。
依赖链：Stage 5.1 → Stage 5.2 → Stage 5.3 → Stage 5.4 → Stage 5.5
```

| Stage | 名称 | 核心交付物 | 建议时长 |
|-------|------|-----------|----------|
| **Stage 5.1** | 模板基础 | 模板实例化框架、Sema 集成 | 1.5 周 |
| **Stage 5.2** | 模板参数推导 | 类型推导、SFINAE、部分特化 | 1.5 周 |
| **Stage 5.3** | 变参模板 | Pack Expansion 实例化 | 1 周 |
| **Stage 5.4** | Concepts | 约束满足、requires 求值 | 1.5 周 |
| **Stage 5.5** | 模板特化与测试 | 显式特化、偏特化、测试 | 1.5 周 |

**Phase 5 架构图：**

```
┌─────────────────────────────────────────────────────────────┐
│                    Template System                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐    │
│  │    AST Layer (已存在)                                │    │
│  │  Decl.h:  TemplateDecl, ClassTemplateDecl,          │    │
│  │            FunctionTemplateDecl, VarTemplateDecl,    │    │
│  │            TemplateTypeParmDecl,                     │    │
│  │            NonTypeTemplateParmDecl,                  │    │
│  │            TemplateTemplateParmDecl,                 │    │
│  │            ConceptDecl,                              │    │
│  │            ClassTemplateSpecializationDecl,          │    │
│  │            ClassTemplatePartialSpecializationDecl    │    │
│  │  Type.h:  TemplateArgument, TemplateArgumentLoc,    │    │
│  │            TemplateTypeParmType,                     │    │
│  │            TemplateSpecializationType,               │    │
│  │            DependentType, AutoType, DecltypeType     │    │
│  │  Expr.h:  TemplateSpecializationExpr,               │    │
│  │            RequiresExpr, CXXFoldExpr,                │    │
│  │            PackIndexingExpr, Requirement 系列        │    │
│  │  TemplateParameterList.h: 模板参数列表               │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │    Parse Layer (已存在)                              │    │
│  │  Parser.h: parseTemplateDeclaration,                 │    │
│  │            parseTemplateParameters,                   │    │
│  │            parseTemplateArgument,                     │    │
│  │            parseConceptDefinition,                    │    │
│  │            parseRequiresClause,                       │    │
│  │            parseConstraintExpression                  │    │
│  └─────────────────────────────────────────────────────┘    │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │    Sema Layer (待实现)                               │    │
│  │  TemplateInstantiation.h  - 模板实例化引擎           │    │
│  │  TemplateDeduction.h      - 模板参数推导             │    │
│  │  SFINAE.h                 - SFINAE 上下文            │    │
│  │  ConstraintSatisfaction.h - 约束满足检查             │    │
│  │  SemaTemplate.cpp         - Sema 模板 ActOn* 方法    │    │
│  └─────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## 已有基础设施盘点

> **重要**：以下 AST 节点、类型、表达式、Parser 方法已在 Phase 0-3 中定义。
> Phase 5 的核心工作是 **Sema 层实现**（模板实例化、参数推导、约束检查）。

### AST 节点 (include/blocktype/AST/Decl.h)

```cpp
namespace blocktype {

// === 模板声明层次 ===
class TemplateDecl : public NamedDecl {
  TemplateParameterList *Params = nullptr;
  Decl *TemplatedDecl;
public:
  TemplateParameterList *getTemplateParameterList() const;
  void setTemplateParameterList(TemplateParameterList *TPL);
  ArrayRef<NamedDecl *> getTemplateParameters() const;
  void addTemplateParameter(NamedDecl *Param);  // 便利方法
  Decl *getTemplatedDecl() const;
  Expr *getRequiresClause() const;
  void setRequiresClause(Expr *E);
  bool hasRequiresClause() const;
  // NodeKind: TemplateDeclKind
};

class FunctionTemplateDecl : public TemplateDecl { /* ... */ };
class ClassTemplateDecl : public TemplateDecl {
  SmallVector<ClassTemplateSpecializationDecl *, 4> Specializations;
public:
  void addSpecialization(ClassTemplateSpecializationDecl *Spec);
  ArrayRef<ClassTemplateSpecializationDecl *> getSpecializations() const;
  // 缺失：findSpecialization(ArrayRef<TemplateArgument>) → 特化查找
};
class VarTemplateDecl : public TemplateDecl { /* 变量模板 */ };
class TypeAliasTemplateDecl : public TemplateDecl { /* 别名模板 */ };

// === 模板参数声明 ===
class TemplateTypeParmDecl : public TypeDecl {
  unsigned Depth, Index;
  bool IsParameterPack, IsTypename;
  QualType DefaultArgument;
public:
  unsigned getDepth() const;
  unsigned getIndex() const;
  bool isParameterPack() const;
  QualType getDefaultArgument() const;
  void setDefaultArgument(QualType T);
};

class NonTypeTemplateParmDecl : public ValueDecl {
  unsigned Depth, Index;
  bool IsParameterPack;
  Expr *DefaultArg;
public:
  unsigned getDepth() const;
  unsigned getIndex() const;
  bool isParameterPack() const;
  Expr *getDefaultArgument() const;
  void setDefaultArgument(Expr *Arg);
  bool hasDefaultArgument() const;
};

class TemplateTemplateParmDecl : public TemplateDecl {
  unsigned Depth, Index;
  bool IsParameterPack;
  TemplateDecl *DefaultArg;
  Expr *Constraint;
public:
  unsigned getDepth() const;
  unsigned getIndex() const;
  bool isParameterPack() const;
  TemplateDecl *getDefaultArgument() const;
  void setDefaultArgument(TemplateDecl *Arg);
  Expr *getConstraint() const;
  void setConstraint(Expr *C);
};

// === 模板特化声明 ===
class ClassTemplateSpecializationDecl : public CXXRecordDecl {
  ClassTemplateDecl *SpecializedTemplate;
  SmallVector<TemplateArgument, 4> TemplateArgs;
  bool IsExplicitSpecialization;
public:
  ClassTemplateDecl *getSpecializedTemplate() const;
  ArrayRef<TemplateArgument> getTemplateArgs() const;
  unsigned getNumTemplateArgs() const;
  const TemplateArgument &getTemplateArg(unsigned Idx) const;
  bool isExplicitSpecialization() const;
};

class ClassTemplatePartialSpecializationDecl
    : public ClassTemplateSpecializationDecl {
  TemplateParameterList *Params;
public:
  void setTemplateParameterList(TemplateParameterList *TPL);
  TemplateParameterList *getTemplateParameterList() const;
  ArrayRef<NamedDecl *> getTemplateParameters() const;
};

class VarTemplateSpecializationDecl : public VarDecl { /* ... */ };
class VarTemplatePartialSpecializationDecl : public VarTemplateSpecializationDecl { /* ... */ };

// === Concept 声明 ===
class ConceptDecl : public TypeDecl {
  Expr *ConstraintExpr;
  TemplateDecl *Template;
public:
  Expr *getConstraintExpr() const;
  TemplateDecl *getTemplate() const;
};

} // namespace blocktype
```

### 类型系统 (include/blocktype/AST/Type.h)

```cpp
namespace blocktype {

// === 模板参数类型 ===
class TemplateTypeParmType : public Type {
  TemplateTypeParmDecl *Decl;
  unsigned Index, Depth;
  bool IsParameterPack;
public:
  TemplateTypeParmDecl *getDecl() const;
  unsigned getIndex() const;
  unsigned getDepth() const;
  bool isParameterPack() const;
};

// === 依赖类型 ===
class DependentType : public Type {
  const Type *BaseType;
  StringRef Name; // T::name
public:
  const Type *getBaseType() const;
  StringRef getName() const;
};

// === 模板特化类型 ===
class TemplateSpecializationType : public Type {
  TemplateDecl *Template;
  SmallVector<TemplateArgument, 4> TemplateArgs;
  StringRef TemplateName;
public:
  TemplateDecl *getTemplateDecl() const;
  StringRef getTemplateName() const;
  ArrayRef<TemplateArgument> getTemplateArgs() const;
  void addTemplateArg(const TemplateArgument &Arg);
  unsigned getNumTemplateArgs() const;
};

// === 模板实参 ===
enum class TemplateArgumentKind {
  Null, Type, Declaration, NullPtr, Integral,
  Template, TemplateExpansion, Expression, Pack
};

class TemplateArgument {
  // 8 种实参类型的完整支持
  // 复制/移动构造函数已显式定义（APSInt 非平凡）
  bool isPackExpansion() const;
  void setPackExpansion(bool IsPack = true);
  // 获取器: getAsType(), getAsExpr(), getAsIntegral(), getAsPack(), ...
};

class TemplateArgumentLoc {
  TemplateArgument Argument;
  SourceLocation Location;
public:
  const TemplateArgument &getArgument() const;
  SourceLocation getLocation() const;
};

// === 自动推导类型 ===
class AutoType : public Type {
  QualType DeducedType;
  bool IsDeduced;
public:
  QualType getDeducedType() const;
  bool isDeduced() const;
  void setDeducedType(QualType T);
};

class DecltypeType : public Type {
  Expr *Expression;
  QualType UnderlyingType;
public:
  Expr *getExpression() const;
  QualType getUnderlyingType() const;
  void setUnderlyingType(QualType T);
};

} // namespace blocktype
```

### 表达式节点 (include/blocktype/AST/Expr.h)

```cpp
namespace blocktype {

class TemplateSpecializationExpr : public Expr {
  StringRef TemplateName;
  SmallVector<TemplateArgument, 4> TemplateArgs;
  ValueDecl *TemplateDecl;
public:
  StringRef getTemplateName() const;
  ArrayRef<TemplateArgument> getTemplateArgs() const;
  ValueDecl *getTemplateDecl() const;
  bool isTypeDependent() const override;
};

// === Requires 表达式 (C++20) ===
class Requirement {
public:
  enum RequirementKind { Type, SimpleExpr, Nested, Compound };
  virtual ~Requirement() = default;
  RequirementKind getKind() const;
};

class TypeRequirement : public Requirement { /* ... */ };
class ExprRequirement : public Requirement { /* ... */ };
class CompoundRequirement : public Requirement { /* ... */ };
class NestedRequirement : public Requirement { /* ... */ };

class RequiresExpr : public Expr {
  SmallVector<Requirement *, 4> Requirements;
public:
  ArrayRef<Requirement *> getRequirements() const;
};

// === Fold 表达式 (C++17) ===
class CXXFoldExpr : public Expr {
  Expr *LHS, *RHS, *Pattern;
  BinaryOpKind Op;
  bool IsRightFold;
public:
  Expr *getLHS() const;
  Expr *getRHS() const;
  Expr *getPattern() const;
  BinaryOpKind getOperator() const;
  bool isRightFold() const;
};

// === Pack 索引 (C++26) ===
class PackIndexingExpr : public Expr {
  Expr *Pack, *Index;
public:
  Expr *getPack() const;
  Expr *getIndex() const;
};

} // namespace blocktype
```

### Parser 已有方法 (include/blocktype/Parse/Parser.h)

```cpp
class Parser {
  // 模板解析（已声明）
  DeclResult parseTemplateDeclaration();
  bool parseTemplateParameters(SmallVector<NamedDecl *, 8> &Params);
  NamedDecl *parseTemplateParameter();
  TemplateTypeParmDecl *parseTemplateTypeParameter();
  NonTypeTemplateParmDecl *parseNonTypeTemplateParameter();
  TemplateTemplateParmDecl *parseTemplateTemplateParameter();
  TemplateArgument parseTemplateArgument();
  bool parseTemplateArgumentList(SmallVector<TemplateArgument, 4> &Args);
  TypeResult parseTemplateId(StringRef Name);
  TypeResult parseTemplateSpecializationType(StringRef Name);
  ExprResult parseTemplateSpecializationExpr(SourceLocation Loc, StringRef Name);
  
  // Concept 解析（已声明）
  ExprResult parseRequiresClause();
  ExprResult parseTypeConstraint();
  ExprResult parseConstraintExpression();
  DeclResult parseConceptDefinition(SourceLocation TemplateLoc,
                                     SmallVector<NamedDecl *, 8> &TemplateParams);
};
```

### ASTContext 已有工厂方法

```cpp
class ASTContext {
  // 类型工厂（已存在）
  TemplateTypeParmType *getTemplateTypeParmType(TemplateTypeParmDecl *, unsigned, unsigned, bool);
  DependentType *getDependentType(const Type *, StringRef);
  TemplateSpecializationType *getTemplateSpecializationType(StringRef);
  FunctionType *getFunctionType(const Type *, ArrayRef<const Type *>, bool, bool, bool);
  QualType getAutoType();
  
  // 节点创建（已存在）
  template <typename T, typename... Args>
  T *create(Args &&...args);  // 所有 AST 节点通过此方法创建
};
```

---

## Stage 5.1 — 模板基础：Sema 集成与实例化框架

### Task 5.1.1 模板 Sema ActOn* 方法

**目标：** 在 Sema 中添加模板相关的语义分析入口方法

**开发要点：**

- **E5.1.1.1** 扩展 `include/blocktype/Sema/Sema.h` 添加模板 ActOn* 方法声明：
  ```cpp
  //===------------------------------------------------------------------===//
  // Template handling [Stage 5.1]
  //===------------------------------------------------------------------===//
  
  /// 处理类模板声明。
  /// @param CTD  已由 Parser 创建的 ClassTemplateDecl
  /// @return     语义分析后的声明结果
  DeclResult ActOnClassTemplateDecl(ClassTemplateDecl *CTD);
  
  /// 处理函数模板声明。
  DeclResult ActOnFunctionTemplateDecl(FunctionTemplateDecl *FTD);
  
  /// 处理变量模板声明。
  DeclResult ActOnVarTemplateDecl(VarTemplateDecl *VTD);
  
  /// 处理别名模板声明。
  DeclResult ActOnTypeAliasTemplateDecl(TypeAliasTemplateDecl *TATD);
  
  /// 处理 Concept 声明。
  DeclResult ActOnConceptDecl(ConceptDecl *CD);
  
  /// 处理模板 ID 引用（如 vector<int>）。
  /// @param Name      模板名
  /// @param Args      模板实参
  /// @param NameLoc   模板名位置
  /// @param LAngleLoc < 位置
  /// @param RAngleLoc > 位置
  /// @return          类型或表达式结果
  TypeResult ActOnTemplateId(llvm::StringRef Name,
                              llvm::ArrayRef<TemplateArgumentLoc> Args,
                              SourceLocation NameLoc,
                              SourceLocation LAngleLoc,
                              SourceLocation RAngleLoc);
  
  /// 处理显式特化声明（template<> class X<T> { ... }）。
  DeclResult ActOnExplicitSpecialization(SourceLocation TemplateLoc,
                                          SourceLocation LAngleLoc,
                                          SourceLocation RAngleLoc);
  
  /// 处理显式实例化声明（template class X<int>;）。
  DeclResult ActOnExplicitInstantiation(SourceLocation TemplateLoc,
                                          Decl *D);
  ```

- **E5.1.1.2** 创建 `src/Sema/SemaTemplate.cpp` 实现上述方法：
  ```cpp
  // src/Sema/SemaTemplate.cpp
  
  DeclResult Sema::ActOnClassTemplateDecl(ClassTemplateDecl *CTD) {
    if (!CTD) return DeclResult::getInvalid();
    // 1. 验证模板参数列表
    // 2. 注册到符号表
    // 3. 处理 requires 子句（如果有）
    Symbols.addDecl(CTD);
    return DeclResult(CTD);
  }
  
  DeclResult Sema::ActOnFunctionTemplateDecl(FunctionTemplateDecl *FTD) {
    if (!FTD) return DeclResult::getInvalid();
    Symbols.addDecl(FTD);
    return DeclResult(FTD);
  }
  
  DeclResult Sema::ActOnConceptDecl(ConceptDecl *CD) {
    if (!CD) return DeclResult::getInvalid();
    Symbols.addDecl(CD);
    return DeclResult(CD);
  }
  ```

**开发关键点提示：**
> 所有 AST 节点（ClassTemplateDecl 等）已由 Parser 通过 `Context.create<T>(...)` 创建。
> Sema 的职责是：验证语义正确性、注册符号、触发实例化。
> 不要重新定义 AST 节点，只添加 Sema 方法。

**Checkpoint：** Sema 模板方法声明完整；能接收 Parser 创建的模板声明

---

### Task 5.1.2 模板实例化框架

**目标：** 实现模板实例化的核心引擎

**开发要点：**

- **E5.1.2.1** 创建 `include/blocktype/Sema/TemplateInstantiation.h`：
  ```cpp
  #pragma once
  
  #include "blocktype/AST/ASTContext.h"
  #include "blocktype/AST/Decl.h"
  #include "blocktype/AST/Type.h"
  
  namespace blocktype {
  
  class Sema;
  
  /// 模板实参映射表。
  /// 维护模板参数到实参的映射关系，用于实例化时的替换。
  class TemplateArgumentList {
    llvm::SmallVector<TemplateArgument, 4> Args;
  
  public:
    TemplateArgumentList() = default;
    explicit TemplateArgumentList(llvm::ArrayRef<TemplateArgument> A)
        : Args(A.begin(), A.end()) {}
  
    unsigned size() const { return Args.size(); }
    bool empty() const { return Args.empty(); }
    llvm::ArrayRef<TemplateArgument> asArray() const { return Args; }
  
    const TemplateArgument &operator[](unsigned I) const { return Args[I]; }
    void push_back(const TemplateArgument &Arg) { Args.push_back(Arg); }
  
    /// 查找参数包实参（用于变参模板展开）。
    /// @return 参数包实参，或空 ArrayRef 如果不存在。
    llvm::ArrayRef<TemplateArgument> getPackArgument() const;
  };
  
  /// 模板实例化引擎。
  /// 负责：
  /// - 类模板实例化（创建 ClassTemplateSpecializationDecl 并填充成员）
  /// - 函数模板实例化（创建实例化后的 FunctionDecl）
  /// - 模板参数替换（递归替换类型、表达式、声明中的模板参数）
  ///
  /// 遵循 Clang 的 TemplateInstantiator 模式：
  /// 实例化是在 Sema 上下文中执行的，所有新节点通过 ASTContext::create() 创建。
  class TemplateInstantiator {
    Sema &SemaRef;
    ASTContext &Context;
  
  public:
    explicit TemplateInstantiator(Sema &S);
  
    // === 类模板实例化 ===
  
    /// 实例化类模板。
    /// @param Template  类模板声明
    /// @param Args      模板实参
    /// @return          特化声明（可能已存在于缓存中）
    ClassTemplateSpecializationDecl *
    InstantiateClassTemplate(ClassTemplateDecl *Template,
                              llvm::ArrayRef<TemplateArgument> Args);
  
    // === 函数模板实例化 ===
  
    /// 实例化函数模板。
    /// @param Template  函数模板声明
    /// @param Args      模板实参
    /// @return          实例化后的函数声明
    FunctionDecl *
    InstantiateFunctionTemplate(FunctionTemplateDecl *Template,
                                  llvm::ArrayRef<TemplateArgument> Args);
  
    // === 参数包展开 ===
  
    /// 展开参数包。
    /// @param Pattern   展开模式（包含对参数包的引用）
    /// @param Args      包含 Pack 类型实参的实参列表
    /// @return          展开后的表达式列表
    llvm::SmallVector<Expr *, 4>
    ExpandPack(Expr *Pattern, const TemplateArgumentList &Args);
  
    // === 类型替换 ===
  
    /// 替换类型中的模板参数为实参。
    /// 例如：将 TemplateTypeParmType(T, index=0) 替换为 int。
    QualType SubstituteType(QualType T, const TemplateArgumentList &Args);
  
    /// 替换表达式中的模板参数。
    Expr *SubstituteExpr(Expr *E, const TemplateArgumentList &Args);
  
    /// 替换声明中的模板参数（用于成员实例化）。
    Decl *SubstituteDecl(Decl *D, const TemplateArgumentList &Args);
  
    // === 特化查找 ===
  
    /// 查找已有的特化。
    /// @return 特化声明，或 nullptr 如果不存在
    ClassTemplateSpecializationDecl *
    FindExistingSpecialization(ClassTemplateDecl *Template,
                                llvm::ArrayRef<TemplateArgument> Args);
  
  private:
    /// 将 TemplateTypeParmType 替换为对应的实参类型。
    QualType SubstituteTemplateTypeParmType(const TemplateTypeParmType *T,
                                             const TemplateArgumentList &Args);
  
    /// 递归替换 TemplateSpecializationType 中的参数。
    QualType SubstituteTemplateSpecializationType(
        const TemplateSpecializationType *T,
        const TemplateArgumentList &Args);
  
    /// 递归替换 DependentType。
    QualType SubstituteDependentType(const DependentType *T,
                                      const TemplateArgumentList &Args);
  };
  
  } // namespace blocktype
  ```

- **E5.1.2.2** 创建 `src/Sema/TemplateInstantiation.cpp` 实现核心逻辑：
  ```cpp
  // 核心方法骨架
  
  ClassTemplateSpecializationDecl *
  TemplateInstantiator::InstantiateClassTemplate(
      ClassTemplateDecl *Template, llvm::ArrayRef<TemplateArgument> Args) {
    // 1. 检查缓存
    if (auto *Spec = FindExistingSpecialization(Template, Args))
      return Spec;
  
    // 2. 通过 ASTContext 创建新的特化声明
    auto *Spec = Context.create<ClassTemplateSpecializationDecl>(
        Template->getLocation(), Template->getName(), Template, Args);
  
    // 3. 获取模板模式（被包装的 CXXRecordDecl）
    CXXRecordDecl *Pattern = llvm::dyn_cast<CXXRecordDecl>(Template->getTemplatedDecl());
    if (!Pattern) return Spec;
  
    // 4. 构建实参映射
    TemplateArgumentList ArgList(Args);
  
    // 5. 遍历模式成员，替换模板参数
    for (Decl *M : Pattern->members()) {
      Decl *InstM = SubstituteDecl(M, ArgList);
      if (InstM) Spec->addMember(InstM);
    }
  
    // 6. 注册特化
    Template->addSpecialization(Spec);
  
    return Spec;
  }
  
  QualType TemplateInstantiator::SubstituteType(
      QualType T, const TemplateArgumentList &Args) {
    if (T.isNull()) return T;
    const Type *Ty = T.getTypePtr();
  
    // TemplateTypeParmType → 查找映射表替换
    if (auto *TTP = llvm::dyn_cast<TemplateTypeParmType>(Ty))
      return SubstituteTemplateTypeParmType(TTP, Args);
  
    // TemplateSpecializationType → 递归替换内部实参
    if (auto *TST = llvm::dyn_cast<TemplateSpecializationType>(Ty))
      return SubstituteTemplateSpecializationType(TST, Args);
  
    // DependentType → 尝试解析
    if (auto *DT = llvm::dyn_cast<DependentType>(Ty))
      return SubstituteDependentType(DT, Args);
  
    // PointerType, ReferenceType → 递归替换 pointee/referenced
    if (auto *PT = llvm::dyn_cast<PointerType>(Ty)) {
      QualType SubPointee = SubstituteType(
          QualType(PT->getPointeeType(), Qualifier::None), Args);
      return QualType(Context.getPointerType(SubPointee.getTypePtr()), T.getQualifiers());
    }
  
    // 非模板参数类型，原样返回
    return T;
  }
  ```

**开发关键点提示：**
> 所有节点通过 `Context.create<T>(...)` 创建，不用 `new`。
> 模板参数替换是递归过程，需处理 Type, Expr, Decl 三种节点。
> ClassTemplateSpecializationDecl 继承自 CXXRecordDecl，有 addMember() 方法。
> TemplateArgumentList 是值类型，可以安全复制。

**Checkpoint：** 模板实例化框架测试通过；能实例化简单类模板 `vector<int>`

---

### Task 5.1.3 模板名称查找与 Sema 集成

**目标：** 将模板实例化引擎集成到 Sema，扩展名称查找

**开发要点：**

- **E5.1.3.1** 在 `include/blocktype/Sema/Sema.h` 添加实例化引擎成员：
  ```cpp
  class Sema {
    // ... 已有成员 ...
    
    /// 模板实例化引擎 [Stage 5.1]
    std::unique_ptr<class TemplateInstantiator> Instantiator;
  
  public:
    /// 访问模板实例化引擎。
    TemplateInstantiator &getTemplateInstantiator() { return *Instantiator; }
  ```

- **E5.1.3.2** 扩展名称查找以处理模板名称：
  ```cpp
  // 在 Sema::LookupUnqualifiedName 中添加模板名称查找逻辑
  // 当 Name 匹配到 ClassTemplateDecl/FunctionTemplateDecl 时，
  // 需要正确返回（而不是报"不是类型"错误）
  
  // 在 Sema::ActOnTemplateId 中：
  // 1. 查找模板名 → 得到 ClassTemplateDecl 或 FunctionTemplateDecl
  // 2. 对实参进行类型检查（数量、类型匹配）
  // 3. 触发 InstantiateClassTemplate 或创建 TemplateSpecializationType
  ```

- **E5.1.3.3** 处理依赖类型查找：
  ```cpp
  // T::type - 依赖名称
  // 检查 isDependentType()：
  //   - 非依赖：立即查找
  //   - 依赖：创建 DependentType，延迟到实例化时解析
  ```

**开发关键点提示：**
> SymbolTable 已支持 TemplateDecl 的 addDecl/lookup。
> 模板名称查找的关键是区分"模板名"和"模板实例"：
>   - `vector` → ClassTemplateDecl（模板名）
>   - `vector<int>` → ClassTemplateSpecializationDecl（实例）
> ActOnTemplateId 是连接 Parser 和实例化引擎的桥梁。

**Checkpoint：** 模板名称查找集成完成；依赖类型正确处理

---

### Task 5.1.4 模板相关诊断 ID

**目标：** 添加模板相关的诊断消息

**开发要点：**

- **E5.1.4.1** 在 `include/blocktype/Basic/DiagnosticSemaKinds.def` 添加：
  ```
  // === Template diagnostics ===
  DIAG(err_template_arg_different_kind, Error,
       "template argument for %0 must be a %1, not a %2",
       "模板参数 %0 必须是 %1，而非 %2")
  DIAG(err_template_arg_num_different, Error,
       "template argument list has %0 arguments, but %1 requires %2",
       "模板实参列表有 %0 个参数，但 %1 需要 %2 个")
  DIAG(err_template_not_in_scope, Error,
       "no template named '%0'",
       "没有名为 '%0' 的模板")
  DIAG(err_template_recursion, Error,
       "recursive template instantiation exceeded maximum depth",
       "递归模板实例化超过最大深度")
  DIAG(err_template_default_arg_not_in_scope, Error,
       "default argument for template parameter '%0' not yet parsed",
       "模板参数 '%0' 的默认实参尚未解析")
  DIAG(note_template_declared_here, Note,
       "template is declared here",
       "模板在此声明")
  DIAG(err_non_type_template_arg_type_mismatch, Error,
       "non-type template argument of type %0 cannot be converted to %1",
       "非类型模板实参类型 %0 无法转换为 %1")
  
  // === Concept diagnostics ===
  DIAG(err_concept_not_satisfied, Error,
       "constraint '%0' not satisfied",
       "约束 '%0' 未被满足")
  DIAG(err_concept_specialization_expr_error, Error,
       "concept specialization expression evaluates to false",
       "concept 特化表达式求值为 false")
  DIAG(note_constraint_substitution_failure, Note,
       "substitution failure: %0",
       "替换失败：%0")
  
  // === Specialization diagnostics ===
  DIAG(err_partial_spec_not_more_specialized, Error,
       "partial specialization is not more specialized than the primary template",
       "偏特化不比主模板更特化")
  DIAG(err_spec_partial_ambiguous, Error,
       "ambiguous partial specializations for %0",
       "%0 的偏特化存在歧义")
  ```

**Checkpoint：** 模板诊断 ID 定义完成

---

## Stage 5.2 — 模板参数推导

### Task 5.2.1 类型推导引擎

**目标：** 实现模板参数推导引擎

**开发要点：**

- **E5.2.1.1** 创建 `include/blocktype/Sema/TemplateDeduction.h`：
  ```cpp
  #pragma once
  
  #include "blocktype/AST/Type.h"
  #include "blocktype/AST/Decl.h"
  #include "llvm/ADT/DenseMap.h"
  
  namespace blocktype {
  
  class Sema;
  
  /// 推导结果码。
  enum class TemplateDeductionResult {
    Success,            // 推导成功
    SubstitutionFailure, // 替换失败（SFINAE）
    NonDeducedMismatch,  // 非推导上下文不匹配
    Inconsistent,       // 已推导参数不一致
    TooManyArguments,   // 实参过多
    TooFewArguments,    // 实参不足
  };
  
  /// 模板推导信息。
  /// 维护每个模板参数的推导结果。
  class TemplateDeductionInfo {
    /// 推导结果：参数索引 → 推导出的实参
    llvm::DenseMap<unsigned, TemplateArgument> DeducedArgs;
  
    /// 第一个失败位置的参数索引（用于诊断）。
    unsigned FirstFailedIndex = ~0u;
  
  public:
    void addDeducedArg(unsigned Index, TemplateArgument Arg) {
      DeducedArgs[Index] = Arg;
    }
  
    TemplateArgument getDeducedArg(unsigned Index) const {
      auto It = DeducedArgs.find(Index);
      return It != DeducedArgs.end() ? It->second : TemplateArgument();
    }
  
    bool hasDeducedArg(unsigned Index) const {
      return DeducedArgs.find(Index) != DeducedArgs.end();
    }
  
    /// 获取所有推导结果（按参数索引排序）。
    llvm::SmallVector<TemplateArgument, 4> getDeducedArgs(unsigned NumParams) const;
  
    unsigned getFirstFailedIndex() const { return FirstFailedIndex; }
    void setFirstFailedIndex(unsigned I) { FirstFailedIndex = I; }
  
    void reset() {
      DeducedArgs.clear();
      FirstFailedIndex = ~0u;
    }
  };
  
  /// 模板参数推导引擎。
  /// 实现 C++ [temp.deduct] 中描述的推导算法。
  class TemplateDeduction {
    Sema &SemaRef;
  
  public:
    explicit TemplateDeduction(Sema &S) : SemaRef(S) {}
  
    // === 函数模板参数推导 ===
  
    /// 从函数调用实参推导函数模板参数。
    /// @param FTD        函数模板声明
    /// @param CallArgs   调用实参表达式列表
    /// @param Info       输出：推导结果
    /// @return           推导结果码
    TemplateDeductionResult
    DeduceFunctionTemplateArguments(FunctionTemplateDecl *FTD,
                                     llvm::ArrayRef<Expr *> CallArgs,
                                     TemplateDeductionInfo &Info);
  
    // === 类型推导 ===
  
    /// 从 P（参数类型）和 A（实参类型）推导模板参数。
    /// 实现 C++ [temp.deduct.type] 中的推导规则。
    /// @param ParamType  函数参数类型（可能包含 TemplateTypeParmType）
    /// @param ArgType    实参表达式的类型
    /// @param Info       推导信息
    /// @return           推导结果码
    TemplateDeductionResult
    DeduceTemplateArguments(QualType ParamType, QualType ArgType,
                             TemplateDeductionInfo &Info);
  
    // === 部分排序 ===
  
    /// 判断 P1 是否比 P2 更特化。
    /// 实现 C++ [temp.deduct.partial] 中的部分排序算法。
    static bool isMoreSpecialized(TemplateDecl *P1, TemplateDecl *P2);
  
  private:
    // 递归推导各种类型组合
    TemplateDeductionResult DeduceFromPointerType(
        const PointerType *P, const PointerType *A, TemplateDeductionInfo &Info);
    TemplateDeductionResult DeduceFromReferenceType(
        const ReferenceType *P, const ReferenceType *A, TemplateDeductionInfo &Info);
    TemplateDeductionResult DeduceFromTemplateSpecializationType(
        const TemplateSpecializationType *P,
        const TemplateSpecializationType *A,
        TemplateDeductionInfo &Info);
  
    // 生成虚拟实参（用于部分排序）
    static QualType generateDeducedType(NamedDecl *Param);
  };
  
  } // namespace blocktype
  ```

- **E5.2.1.2** 创建 `src/Sema/TemplateDeduction.cpp` 实现推导算法：
  ```cpp
  TemplateDeductionResult
  TemplateDeduction::DeduceTemplateArguments(QualType ParamType,
                                               QualType ArgType,
                                               TemplateDeductionInfo &Info) {
    // 1. 完全相同 → 无需推导
    if (ParamType == ArgType) return TemplateDeductionResult::Success;
  
    // 2. TemplateTypeParmType → 直接推导
    if (auto *TTP = llvm::dyn_cast<TemplateTypeParmType>(ParamType.getTypePtr())) {
      if (Info.hasDeducedArg(TTP->getIndex())) {
        // 检查一致性
        return Info.getDeducedArg(TTP->getIndex()).getAsType() == ArgType
                   ? TemplateDeductionResult::Success
                   : TemplateDeductionResult::Inconsistent;
      }
      Info.addDeducedArg(TTP->getIndex(), TemplateArgument(ArgType));
      return TemplateDeductionResult::Success;
    }
  
    // 3. 指针类型 → 递归推导 pointee
    // 4. 引用类型 → 递归推导 referent（含引用折叠）
    // 5. TemplateSpecializationType → 递归推导模板实参
    // 6. 无法推导 → NonDeducedMismatch
    // ...
  }
  ```

**开发关键点提示：**
> TemplateTypeParmType 在 Type.h 中已定义，通过 getDecl() 可获取 TemplateTypeParmDecl。
> TemplateTypeParmDecl::getIndex() 返回参数在列表中的位置，用于 DeducedArgs 映射。
> 引用折叠：T& & → T&, T&& & → T&, T& && → T&, T&& && → T&&。
> cv 限定符：const T 可以从 const int 推导 T = int。

**Checkpoint：** 模板参数推导测试通过

---

### Task 5.2.2 SFINAE 实现

**目标：** 实现 SFINAE（替换失败不是错误）

**开发要点：**

- **E5.2.2.1** 创建 `include/blocktype/Sema/SFINAE.h`：
  ```cpp
  #pragma once
  
  #include "blocktype/AST/Type.h"
  #include "llvm/ADT/SmallVector.h"
  
  namespace blocktype {
  
  /// SFINAE 上下文。
  /// 在模板参数替换过程中，如果处于 SFINAE 上下文，
  /// 替换失败不会产生编译错误，而是从候选集中移除该候选。
  class SFINAEContext {
    bool InSFINAE = false;
    unsigned NestingLevel = 0;
    llvm::SmallVector<std::string, 4> FailureReasons;
  
  public:
    void enterSFINAE() { InSFINAE = true; ++NestingLevel; }
    void exitSFINAE() {
      if (NestingLevel > 0) --NestingLevel;
      if (NestingLevel == 0) InSFINAE = false;
    }
  
    bool isSFINAE() const { return InSFINAE; }
    unsigned getNestingLevel() const { return NestingLevel; }
  
    void addFailureReason(llvm::StringRef Reason) {
      FailureReasons.push_back(Reason.str());
    }
    llvm::ArrayRef<std::string> getFailureReasons() const { return FailureReasons; }
    void clearFailureReasons() { FailureReasons.clear(); }
  };
  
  } // namespace blocktype
  ```

- **E5.2.2.2** 在 TemplateInstantiator 中集成 SFINAE：
  ```cpp
  // SubstituteType 在 SFINAE 上下文中：
  // - 正常上下文：替换失败 → 报编译错误
  // - SFINAE 上下文：替换失败 → 返回空 QualType，不报错
  //
  // 在重载决议中：
  // 1. 进入 SFINAE 上下文
  // 2. 对每个候选尝试替换
  // 3. 替换失败 → 从候选集移除
  // 4. 退出 SFINAE 上下文
  ```

**开发关键点提示：**
> SFINAE 只在"立即上下文"中有效。函数体中的错误是硬错误。
> 典型场景：`decltype(t.size())` — 如果 T 没有 size() 方法，SFINAE 移除候选。
> SFINAEContext 应该在 Sema 中管理，作为成员变量。

**Checkpoint：** SFINAE 测试通过；enable_if 模式正确

---

### Task 5.2.3 部分排序

**目标：** 实现模板部分排序

**开发要点：**

- **E5.2.3.1** 在 `TemplateDeduction.h` 的 `isMoreSpecialized` 中实现：
  ```cpp
  // 部分排序算法：
  // 1. 为 P1 的参数生成虚拟类型（唯一类型 A1, A2, ...）
  // 2. 用虚拟类型尝试推导 P2 的参数
  // 3. 反过来：为 P2 的参数生成虚拟类型推导 P1
  // 4. 如果 P2→P1 成功但 P1→P2 失败，则 P1 更特化
  //
  // 示例：
  //   template<typename T> void f(T)       — 最不特化
  //   template<typename T> void f(T*)      — 较特化（T→T*）
  //   template<>          void f(int*)     — 最特化（显式特化）
  ```

**Checkpoint：** 部分排序测试通过；重载决议正确选择更特化模板

---

## Stage 5.3 — 变参模板

### Task 5.3.1 参数包展开实例化

**目标：** 在 TemplateInstantiator 中实现参数包展开

**开发要点：**

- **E5.3.1.1** 在 `TemplateInstantiator` 中实现 `ExpandPack`：
  ```cpp
  llvm::SmallVector<Expr *, 4>
  TemplateInstantiator::ExpandPack(Expr *Pattern,
                                     const TemplateArgumentList &Args) {
    llvm::SmallVector<Expr *, 4> Result;
  
    // 1. 查找参数包实参
    llvm::ArrayRef<TemplateArgument> PackArgs = Args.getPackArgument();
    if (PackArgs.empty()) return Result;
  
    // 2. 对包中每个元素实例化模式
    for (const TemplateArgument &PA : PackArgs) {
      // 创建只含单个元素的实参列表
      TemplateArgumentList SingleArg;
      SingleArg.push_back(PA);
  
      Expr *Inst = SubstituteExpr(Pattern, SingleArg);
      if (Inst) Result.push_back(Inst);
    }
  
    return Result;
  }
  ```

- **E5.3.1.2** 处理 CXXFoldExpr 实例化：
  ```cpp
  Expr *TemplateInstantiator::InstantiateFoldExpr(
      CXXFoldExpr *FE, const TemplateArgumentList &Args) {
    // (args + ...) → 展开为 args[0] + args[1] + ... + args[n-1]
    llvm::SmallVector<Expr *, 4> Elems = ExpandPack(FE->getPattern(), Args);
  
    if (Elems.empty()) {
      // 空包：返回单位元
      // +→0, *→1, &&→true, ||→false, ,→void()
      return getIdentityElement(FE->getOperator());
    }
  
    // 构建二元表达式链
    Expr *Result = Elems[0];
    for (unsigned I = 1; I < Elems.size(); ++I) {
      Result = Context.create<BinaryOperator>(
          FE->getLocation(), Result, Elems[I], FE->getOperator());
    }
    return Result;
  }
  ```

**开发关键点提示：**
> TemplateArgument::isPack() 和 getAsPack() 已在 Type.h 中定义。
> CXXFoldExpr 在 Expr.h 中已定义，有 getOperator(), getPattern(), getLHS(), getRHS()。
> 展开规则：所有参数包同时展开，长度必须相同。

**Checkpoint：** 参数包展开测试通过；tuple<int, double, char> 模式正确

---

### Task 5.3.2 PackIndexingExpr 实例化

**目标：** 实现 C++26 pack indexing 表达式求值

**开发要点：**

- **E5.3.2.1** 在 TemplateInstantiator 中处理 PackIndexingExpr：
  ```cpp
  // Ts...[0] → 展开参数包后取第 0 个元素
  // 1. 获取 Pack 实参
  // 2. 求值 Index 表达式（必须是常量）
  // 3. 返回对应位置的元素
  ```

**Checkpoint：** pack indexing 测试通过

---

## Stage 5.4 — Concepts

### Task 5.4.1 Concept 约束满足检查

**目标：** 实现 C++20 Concept 约束满足检查

**开发要点：**

- **E5.4.1.1** 创建 `include/blocktype/Sema/ConstraintSatisfaction.h`：
  ```cpp
  #pragma once
  
  #include "blocktype/AST/Decl.h"
  #include "blocktype/AST/Expr.h"
  #include "blocktype/Sema/TemplateInstantiation.h"
  
  namespace blocktype {
  
  class Sema;
  
  /// 约束满足检查器。
  /// 实现 C++20 [concept] 中的约束满足语义。
  class ConstraintSatisfaction {
    Sema &SemaRef;
  
  public:
    explicit ConstraintSatisfaction(Sema &S) : SemaRef(S) {}
  
    /// 检查约束表达式是否被满足。
    /// @param Constraint 约束表达式（通常来自 ConceptDecl 或 requires 子句）
    /// @param Args       模板实参列表（用于替换约束中的参数）
    /// @return true 如果约束被满足
    bool CheckConstraintSatisfaction(Expr *Constraint,
                                      const TemplateArgumentList &Args);
  
    /// 检查 Concept 是否被满足。
    /// @param Concept Concept 声明
    /// @param Args    模板实参
    /// @return true 如果 concept 被满足
    bool CheckConceptSatisfaction(ConceptDecl *Concept,
                                   llvm::ArrayRef<TemplateArgument> Args);
  
    /// 求值 requires 表达式。
    /// @param RE requires 表达式
    /// @return true 如果所有要求都被满足
    bool EvaluateRequiresExpr(RequiresExpr *RE);
  
  private:
    /// 求值单个 Requirement。
    bool EvaluateRequirement(Requirement *R, const TemplateArgumentList &Args);
  
    /// 求值原子约束表达式为 bool 值。
    /// @return std::optional<bool> — 如果求值成功返回 bool，否则 std::nullopt
    std::optional<bool> EvaluateConstraintExpr(Expr *E);
  };
  
  } // namespace blocktype
  ```

- **E5.4.1.2** 在 Sema 中集成约束检查：
  ```cpp
  // 在 Sema.h 中添加：
  std::unique_ptr<class ConstraintSatisfaction> ConstraintChecker;
  
  // 在模板参数推导完成后：
  // 1. 获取模板的 requires 子句（如果有）
  // 2. 调用 ConstraintChecker->CheckConstraintSatisfaction
  // 3. 如果不满足，从候选集移除（类似 SFINAE）
  ```

**开发关键点提示：**
> ConceptDecl 已在 Decl.h 中定义，有 getConstraintExpr() 和 getTemplate()。
> RequiresExpr, Requirement, TypeRequirement, ExprRequirement, CompoundRequirement, NestedRequirement
> 全部已在 Expr.h 中定义。
> 约束合取/析取：C1 && C2 → 两者都满足，C1 || C2 → 至少一个满足。
> Concept 部分排序：更严格约束的模板更特化。

**Checkpoint：** 约束满足检查测试通过

---

### Task 5.4.2 Requires 表达式求值

**目标：** 实现 requires 表达式的求值

**开发要点：**

- **E5.4.2.1** 在 `ConstraintSatisfaction` 中实现 `EvaluateRequiresExpr`：
  ```cpp
  bool ConstraintSatisfaction::EvaluateRequiresExpr(RequiresExpr *RE) {
    for (Requirement *R : RE->getRequirements()) {
      if (!EvaluateRequirement(R, /* Args */ {}))
        return false;
    }
    return true;
  }
  
  bool ConstraintSatisfaction::EvaluateRequirement(
      Requirement *R, const TemplateArgumentList &Args) {
    switch (R->getKind()) {
    case Requirement::RequirementKind::Type: {
      auto *TR = llvm::dyn_cast<TypeRequirement>(R);
      // 类型必须有效（非依赖、完整类型）
      return !TR->getType().isNull();
    }
    case Requirement::RequirementKind::SimpleExpr: {
      auto *ER = llvm::dyn_cast<ExprRequirement>(R);
      // 表达式必须有效（类型检查通过）
      // noexcept 要求：表达式不能抛出异常
      return true; // 简化：假设有效
    }
    case Requirement::RequirementKind::Compound: {
      auto *CR = llvm::dyn_cast<CompoundRequirement>(R);
      // { expression } -> type-constraint
      // 检查表达式类型是否满足返回类型约束
      return true;
    }
    case Requirement::RequirementKind::Nested: {
      auto *NR = llvm::dyn_cast<NestedRequirement>(R);
      // requires constraint-expression → 递归求值
      auto Result = EvaluateConstraintExpr(NR->getConstraint());
      return Result.has_value() && Result.value();
    }
    }
    return false;
  }
  ```

**Checkpoint：** Requires 表达式求值测试通过

---

## Stage 5.5 — 模板特化与测试

### Task 5.5.1 显式特化

**目标：** 实现模板显式特化

**开发要点：**

- **E5.5.1.1** 在 `Sema::ActOnExplicitSpecialization` 中实现：
  ```cpp
  DeclResult Sema::ActOnExplicitSpecialization(
      SourceLocation TemplateLoc,
      SourceLocation LAngleLoc,
      SourceLocation RAngleLoc) {
    // template<> 意味着空模板参数列表
    // 1. 解析后面的声明（class/function/variable）
    // 2. 查找主模板
    // 3. 验证实参与主模板参数匹配
    // 4. 创建 SpecializationDecl 并标记为 IsExplicitSpecialization
  }
  ```
  > ClassTemplateSpecializationDecl 已有 `isExplicitSpecialization()` 方法。
  > VarTemplateSpecializationDecl 也已有 `isExplicitSpecialization()` 方法。

**Checkpoint：** 显式特化测试通过

---

### Task 5.5.2 偏特化

**目标：** 实现模板偏特化

**开发要点：**

- **E5.5.2.1** 在 Sema 中处理偏特化声明：
  ```cpp
  // template<typename T> class vector<T*> { ... };
  // 1. ClassTemplatePartialSpecializationDecl 已在 Decl.h 中定义
  // 2. 验证偏特化参数是主模板参数的子集
  // 3. 使用部分排序确定特化程度
  // 4. 注册到 ClassTemplateDecl
  ```
  > ClassTemplatePartialSpecializationDecl 已有 `setTemplateParameterList()` 和 `getTemplateParameters()` 方法。
  > VarTemplatePartialSpecializationDecl 也已定义。

**Checkpoint：** 偏特化测试通过

---

### Task 5.5.3 模板测试

**目标：** 建立模板系统的完整测试覆盖

**开发要点：**

- **E5.5.3.1** 创建测试文件：
  ```
  tests/unit/Sema/
  ├── TemplateInstantiationTest.cpp    # 模板实例化测试
  ├── TemplateDeductionTest.cpp       # 模板参数推导测试
  ├── SFINAETest.cpp                  # SFINAE 测试
  ├── ConceptTest.cpp                 # Concepts 测试
  └── VariadicTemplateTest.cpp        # 变参模板测试
  ```

- **E5.5.3.2** 创建回归测试：
  ```
  tests/lit/Sema/
  ├── basic-template.test             # 基础模板
  ├── specialization.test            # 特化
  ├── variadic.test                   # 变参模板
  ├── sfinae.test                     # SFINAE
  ├── concepts.test                   # Concepts
  └── std-library.test               # 标准库兼容
  ```

**测试覆盖重点：**

| 模块 | 核心测试用例 |
|------|-------------|
| TemplateInstantiator | 类模板实例化、函数模板实例化、递归实例化、缓存验证 |
| TemplateDeduction | 精确匹配、指针推导、引用折叠、cv 限定符、非推导上下文 |
| SFINAE | enable_if 模式、void_t 检测、替换失败 vs 硬错误 |
| ConstraintSatisfaction | concept 满足/不满足、合取/析取、requires 表达式 |
| VariadicTemplate | 参数包展开、fold 表达式、空包处理 |

**Checkpoint：** 测试覆盖率 ≥ 80%；所有测试通过

---

## 📋 Phase 5 验收检查清单

```
[ ] Sema 模板 ActOn* 方法声明并实现
[ ] TemplateInstantiator 类创建完成
[ ] 模板参数替换（SubstituteType/Expr/Decl）实现
[ ] 模板实例化缓存机制实现
[ ] 模板名称查找集成完成
[ ] 模板相关诊断 ID 定义完成
[ ] TemplateDeduction 推导引擎实现
[ ] SFINAE 上下文管理实现
[ ] 部分排序算法实现
[ ] 参数包展开实例化实现
[ ] Fold 表达式实例化实现
[ ] ConstraintSatisfaction 检查器实现
[ ] Requires 表达式求值实现
[ ] 显式特化处理实现
[ ] 偏特化处理实现
[ ] 测试覆盖率 ≥ 80%
```

---

## 📁 Phase 5 新增文件清单

```
include/blocktype/Sema/
├── TemplateInstantiation.h    # 模板实例化引擎 + TemplateArgumentList
├── TemplateDeduction.h        # 模板参数推导引擎 + TemplateDeductionInfo
├── SFINAE.h                   # SFINAE 上下文
└── ConstraintSatisfaction.h   # 约束满足检查器

src/Sema/
├── SemaTemplate.cpp           # Sema 模板 ActOn* 方法实现
├── TemplateInstantiation.cpp  # 模板实例化实现
├── TemplateDeduction.cpp      # 模板参数推导实现
└── ConstraintSatisfaction.cpp # 约束满足实现

src/Sema/CMakeLists.txt        # 更新：添加新源文件

include/blocktype/Sema/Sema.h  # 更新：添加模板相关方法声明
include/blocktype/Basic/DiagnosticSemaKinds.def  # 更新：添加模板诊断 ID
```

---

*Phase 5 完成标志：模板系统完整实现；能编译标准库级别的模板代码；Concepts、变参模板、SFINAE 等核心功能就绪；测试通过，覆盖率 ≥ 80%。*
