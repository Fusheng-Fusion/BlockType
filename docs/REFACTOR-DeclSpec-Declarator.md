# 重构方案: DeclSpec / Declarator / DeclaratorChunk 架构

> 目标: 引入结构化的声明解析架构，替代当前 `parseDeclaration()` 中的即席(ad-hoc)解析方式，
> 为 Phase 4 Sema 语义分析提供结构化的声明信息。

---

## 一、现状分析

### 1.1 当前架构问题

| 问题 | 现状 | 影响 |
|------|------|------|
| 无 DeclSpec | `IsStatic`/`IsConstexpr`/`IsInline` 作为局部 bool 变量 | 缺少 `friend`/`virtual`/`explicit`/`mutable`/存储类等说明符 |
| 无 DeclaratorChunk | `parseDeclarator()` 直接修改 `QualType` 构建 Type 链 | 无法区分 "先指针后数组" vs "先数组后指针"；无法处理 `int (*pf)(int)` |
| 无 DeclarationName | 使用 `llvm::StringRef` | 无法表示 operator+、构造/析构函数名、转换函数名 |
| 不支持括号改变绑定 | `parseDeclarator()` 先解析所有指针，再解析数组/函数 | `int (*arr)[10]` 被错误解析为 `int *arr[10]` |
| 无多声明符 | `int a, *b, c[10];` 不支持 | Sema 无法处理逗号分隔的声明列表 |

### 1.2 当前调用链

```
parseDeclaration()
  ├── [即席] 解析 static/constexpr/inline → 局部 bool
  ├── parseType() → parseTypeSpecifier() + parseDeclarator(QualType)
  │     └── parseDeclarator(): while(star/amp/ampamp) → 修改 QualType
  │         └── while(l_square/l_paren) → 数组/函数类型
  ├── [即席] 读取 identifier 名称
  └── 根据 l_paren/other 分派到 parseFunctionDeclaration() 或 parseVariableDeclaration()
```

### 1.3 目标调用链

```
parseDeclaration()
  ├── parseDeclSpecifierSeq(DS)  → 填充 DeclSpec (类型+说明符)
  ├── parseDeclarator(D)         → 填充 Declarator (名字+声明符链)
  │     ├── DeclaratorChunk::Pointer/Reference
  │     ├── '(' Declarator ')'   → 括号改变绑定
  │     ├── DeclaratorChunk::Array
  │     ├── DeclaratorChunk::Function
  │     └── DeclaratorChunk::MemberPointer
  └── 根据 D 内容构建 VarDecl/FunctionDecl/etc.
```

---

## 二、数据结构设计

### 2.1 StorageClass (存储类)

```cpp
enum class StorageClass {
  None,       // 未指定
  Auto,       // auto (C 语言含义，非 C++11)
  Register,   // register
  Static,     // static
  Extern,     // extern
  Mutable,    // mutable (仅类成员)
  ThreadLocal // thread_local
};
```

### 2.2 DeclSpec (声明说明符)

```cpp
struct DeclSpec {
  // === 类型说明符 ===
  QualType Type;                    // 解析得到的类型 (可能为空表示解析失败)
  SourceLocation TypeLoc;           // 类型起始位置

  // === 存储类 ===
  StorageClass SC = StorageClass::None;
  SourceLocation SCLoc;

  // === 函数说明符 ===
  bool IsInline : 1 = false;
  bool IsVirtual : 1 = false;
  bool IsExplicit : 1 = false;      // explicit / explicit(true) / explicit(false)
  SourceLocation InlineLoc, VirtualLoc, ExplicitLoc;

  // === 其他说明符 ===
  bool IsFriend : 1 = false;
  bool IsConstexpr : 1 = false;
  bool IsConsteval : 1 = false;
  bool IsConstinit : 1 = false;
  SourceLocation FriendLoc, ConstexprLoc, ConstevalLoc, ConstinitLoc;

  // === 类型修饰 ===
  // (const/volatile/restrict 已在 QualType 中)

  // === 方法 ===
  bool hasTypeSpecifier() const { return !Type.isNull(); }
  void finish(DiagnosticsEngine &Diags);  // 验证说明符组合合法性
};
```

### 2.3 DeclarationName (声明名称)

```cpp
class DeclarationName {
public:
  enum class NameKind {
    Identifier,           // 普通标识符: x, foo
    CXXConstructorName,   // 构造函数: ClassName()
    CXXDestructorName,    // 析构函数: ~ClassName()
    CXXConversionFunctionName, // operator type()
    CXXOperatorName,      // operator+ 等
    CXXLiteralOperatorName, // operator""_s
    CXXDeductionGuideName,  // 类模板推导指引
    CXXUsingDirective,    // using namespace
  };

private:
  NameKind Kind = NameKind::Identifier;
  llvm::StringRef Name;   // 标识符文本 (仅 Identifier)
  QualType NamedType;     // 构造/析构/转换的目标类型

public:
  DeclarationName() = default;
  explicit DeclarationName(llvm::StringRef Name) : Name(Name) {}

  NameKind getKind() const { return Kind; }
  llvm::StringRef getAsString() const;

  bool isEmpty() const { return Kind == NameKind::Identifier && Name.empty(); }
  bool isIdentifier() const { return Kind == NameKind::Identifier; }
  llvm::StringRef getIdentifier() const { return Name; }
};
```

### 2.4 DeclaratorChunk (声明符片段)

```cpp
class DeclaratorChunk {
public:
  enum ChunkKind {
    Pointer,        // * / *const / *volatile
    Reference,      // & / &&
    Array,          // [expr?] / [size]
    Function,       // (params) cvr-qualifiers ref-qualifier
    MemberPointer,  // Class::*
  };

private:
  ChunkKind Kind;

  // Pointer / MemberPointer info
  Qualifier CVRQuals = Qualifier::None;
  SourceLocation Loc;

  // Array info
  Expr *ArraySize = nullptr;    // 常量表达式 (nullptr = 不完整数组 int[])

  // Function info
  struct FunctionInfo {
    llvm::SmallVector<QualType, 4> ParamTypes;
    bool IsVariadic = false;
    Qualifier MethodQuals = Qualifier::None;
    bool HasRefQualifier = false;
    bool IsRValueRef = false;
  };
  FunctionInfo Func;

  // MemberPointer info
  llvm::StringRef ClassQualifier;  // 类名限定符

public:
  static DeclaratorChunk getPointer(Qualifier CVR, SourceLocation Loc);
  static DeclaratorChunk getReference(bool IsRValue, SourceLocation Loc);
  static DeclaratorChunk getArray(Expr *Size, SourceLocation Loc);
  static DeclaratorChunk getFunction(FunctionInfo Info, SourceLocation Loc);
  static DeclaratorChunk getMemberPointer(llvm::StringRef Class, Qualifier CVR,
                                          SourceLocation Loc);

  ChunkKind getKind() const { return Kind; }
};
```

### 2.5 Declarator (声明符)

```cpp
enum class DeclaratorContext {
  FileContext,        // 文件级声明
  BlockContext,       // 块级声明 (函数体内)
  MemberContext,      // 类成员声明
  ParameterContext,   // 函数参数
  TemplateParamContext, // 模板参数 (非类型模板参数)
  ConditionContext,   // 条件表达式中的声明 (if/while/for)
  ConversionIdContext, // 转换函数 ID
  CXXNewContext,      // new 表达式中的类型
  TypeNameContext,    // 纯类型名 (无声明符)
};

class Declarator {
  DeclSpec DS;                                    // 声明说明符
  llvm::SmallVector<DeclaratorChunk, 8> Chunks;   // 声明符链 (从内到外)
  DeclarationName Name;                           // 声明名称
  SourceLocation NameLoc;                         // 名称位置
  SourceRange Range;                              // 整个声明符范围
  DeclaratorContext Context;                      // 声明上下文

  // 初始化器 (仅用于变量声明)
  Expr *Init = nullptr;

  // 函数体 (仅用于函数定义)
  Stmt *Body = nullptr;

  // 参数默认值 (仅用于函数参数)
  llvm::SmallVector<std::pair<ParmVarDecl*, Expr*>, 8> DefaultArgs;

public:
  Declarator(DeclSpec &DS, DeclaratorContext Ctx);

  // 添加声明符片段
  void addChunk(DeclaratorChunk Chunk);

  // 设置名称
  void setName(DeclarationName N, SourceLocation Loc);

  // 查询
  bool isFunctionDeclarator() const;
  bool isPointerOrReference() const;
  unsigned getNumChunks() const { return Chunks.size(); }

  // 构建最终 QualType (从 Chunks 反向应用)
  QualType buildType(ASTContext &Ctx) const;

  // 访问器
  const DeclSpec &getDeclSpec() const { return DS; }
  DeclarationName getName() const { return Name; }
  SourceLocation getNameLoc() const { return NameLoc; }
  DeclaratorContext getContext() const { return Context; }
  llvm::ArrayRef<DeclaratorChunk> chunks() const { return Chunks; }
};
```

---

## 三、分步实施计划

### Step 1: 创建 DeclSpec 数据结构
**文件**: `include/blocktype/Parse/DeclSpec.h`
**内容**: StorageClass 枚举 + DeclSpec 结构体
**依赖**: 无 (仅依赖 QualType, SourceLocation, Diagnostics)
**影响**: 新文件，不影响现有代码

### Step 2: 创建 DeclaratorChunk 数据结构
**文件**: `include/blocktype/Parse/DeclaratorChunk.h`
**内容**: DeclaratorChunk 类 (5 种 Kind + 工厂方法)
**依赖**: QualType, Expr, SourceLocation
**影响**: 新文件，不影响现有代码

### Step 3: 创建 Declarator 数据结构
**文件**: `include/blocktype/Parse/Declarator.h`
**内容**: DeclarationName + DeclaratorContext 枚举 + Declarator 类
**依赖**: DeclSpec.h, DeclaratorChunk.h
**影响**: 新文件，不影响现有代码

### Step 4: 创建 DeclarationName 类
**文件**: 与 Declarator.h 同文件
**内容**: DeclarationName 类 (支持标识符/构造/析构/operator 名称)
**依赖**: llvm::StringRef, QualType
**影响**: 新文件，不影响现有代码

### Step 5: 实现 parseDeclSpecifierSeq()
**文件**: `src/Parse/ParseDeclSpec.cpp` (新文件)
**内容**: 替代 `parseDeclaration()` 中的即席 `while(kw_static/kw_constexpr/kw_inline)` 循环
**函数签名**: `void Parser::parseDeclSpecifierSeq(DeclSpec &DS)`
**功能**:
  - 解析存储类: static/extern/register/mutable/thread_local
  - 解析函数说明符: inline/virtual/explicit
  - 解析 friend/constexpr/consteval/constinit
  - 解析类型说明符: 复用现有 `parseTypeSpecifier()` 逻辑
  - 错误检查: 互斥说明符、重复说明符
**共存**: 旧路径保留，新函数暂不被调用

### Step 6: 重构 parseDeclarator()
**文件**: `src/Parse/ParseType.cpp`
**变更**: 新增 `void Parser::parseDeclarator(Declarator &D)` 重载
**保留**: 旧 `QualType parseDeclarator(QualType Base)` 暂时保留
**新逻辑**:
```
parseDeclarator(Declarator &D):
  // 1. 解析前缀: pointer/reference/member-pointer
  while (* / & / && / ClassName::*):
    D.addChunk(DeclaratorChunk::getPointer/Reference/MemberPointer(...))

  // 2. 解析直接声明符
  if identifier:
    D.setName(DeclarationName(text), loc)
  elif '(':
    // 递归解析括号内的子声明符 (支持 int (*pf)(int))
    parseDeclaratorInParens(D)

  // 3. 解析后缀: array/function
  while ('[' / '('):
    D.addChunk(DeclaratorChunk::getArray/Function(...))
```

### Step 7: 重构 parseDeclaration() 主路径
**文件**: `src/Parse/ParseDecl.cpp`
**变更**: 在一般变量/函数声明路径中，替换即席解析为:
```cpp
// 旧代码:
//   bool IsStatic = false, IsConstexpr = false, IsInline = false;
//   while (kw_static/kw_constexpr/kw_inline) { ... }
//   QualType Type = parseType();
//   Name = Tok.getText();
// 新代码:
DeclSpec DS;
parseDeclSpecifierSeq(DS);
Declarator D(DS, DeclaratorContext::FileContext);
parseDeclarator(D);
QualType T = D.buildType(Context);
```
**共存策略**: 通过编译开关或逐步替换，每次只替换一个调用点

### Step 8: 重构 parseVariableDeclaration/parseFunctionDeclaration
**文件**: `src/Parse/ParseDecl.cpp`
**变更**: 新增从 `Declarator` 构建 AST 节点的辅助函数:
- `VarDecl *buildVarDecl(Declarator &D)` — 替代 `parseVariableDeclaration()`
- `FunctionDecl *buildFunctionDecl(Declarator &D)` — 替代 `parseFunctionDeclaration()`
**注意**: 参数列表解析需从 `DeclaratorChunk::Function` 中提取

### Step 9: 支持复杂声明符
**此步骤是架构重构的核心收益**:
- `int (*pf)(int)` — 括号改变绑定: DeclaratorChunk::Pointer 后跟 DeclaratorChunk::Function
- `int (Class::*pmf)(int)` — 成员指针函数: DeclaratorChunk::MemberPointer + Function
- `int (*arr)[10]` — 数组指针: Pointer + Array (而不是 Array + Pointer)
- `int *ap[10]` — 指针数组: Array + Pointer (正确!)
- `int a, *b, c[10]` — 多声明符: 循环 parseDeclarator()

### Step 10: 迁移 parseClassMember() 中的成员声明
**文件**: `src/Parse/ParseDecl.cpp`
**当前**: `parseClassMember()` 中有独立的即席解析路径:
```cpp
// 当前 parseClassMember 的 "一般数据成员/成员函数" 路径:
bool IsStatic = false, IsMutable = false, IsVirtual = false;
while (kw_static / kw_mutable / kw_virtual) { ... }
QualType Type = parseType();
// ... 手动分派到函数/字段 ...
```
**迁移为**:
```cpp
DeclSpec DS;
parseDeclSpecifierSeq(DS);
Declarator D(DS, DeclaratorContext::MemberContext);
parseDeclarator(D);
// 从 D 判断是 FieldDecl 还是 MethodDecl
```

### Step 11: 迁移 typedef/非类型模板参数等辅助调用点
以下调用点使用 `parseType()` + 旧 `parseDeclarator(QualType)`:
- `parseTypedefDeclaration()`: `Type = parseType(); Type = parseDeclarator(Type);` → 使用 Declarator
- `parseNonTypeTemplateParameter()`: `Type = parseType(); Type = parseDeclarator(Type);` → 使用 Declarator
- `parseParameterDeclaration()`: `QualType Type = parseType();` → 使用 DeclSpec + Declarator

### Step 12: 迁移 ParseStmt.cpp 中的声明位置
- `parseForStatement()` (line 581, 600, 711): `for` 循环中的声明
- `parseIfStatement()` / `parseWhileStatement()` 中的条件声明
**迁移为**: `parseDeclSpecifierSeq()` + `parseDeclarator()` 替代 `parseType()` 即席解析

### Step 13: 构建测试并验证
- 复杂函数指针: `int (*pf)(int, double)`
- 成员指针: `int Class::*ptr`
- 成员函数指针: `int (Class::*pmf)(int) const`
- 数组指针: `int (*arr)[10]`
- 指针数组: `int *ap[10]`
- 多声明符: `int a, *b, c[10]`
- const 指针: `int *const p`, `const int *p`, `int const *p`
- 指向函数指针的引用: `int (*(&rf))(int)`
- 运行所有现有测试确保无回归

### Step 14: 完全迁移 — 清除旧代码
**目标**: 删除所有旧路径代码，实现完全迁移。

#### 14.1 删除旧函数
| 要删除的旧函数/签名 | 文件 | 替代 |
|------|------|------|
| `QualType Parser::parseDeclarator(QualType Base)` | ParseType.cpp, Parser.h | `void Parser::parseDeclarator(Declarator &D)` |
| `VarDecl *Parser::parseVariableDeclaration(QualType, StringRef, SourceLocation, bool, bool)` | ParseDecl.cpp, Parser.h | `VarDecl *Parser::buildVarDecl(Declarator &D)` |
| `FunctionDecl *Parser::parseFunctionDeclaration(QualType, StringRef, SourceLocation, bool, bool, bool)` | ParseDecl.cpp, Parser.h | `FunctionDecl *Parser::buildFunctionDecl(Declarator &D)` |
| `QualType Parser::parseType()` 中调用旧 `parseDeclarator(QualType)` 的路径 | ParseType.cpp | `parseType()` 改为调用 `parseTypeSpecifier()` 仅返回基类型 |

#### 14.2 修改 parseType()
`parseType()` 当前做两件事: 解析类型说明符 + 解析声明符。
迁移后 `parseType()` 的职责需要明确:
```
// 选项 A: parseType() 只解析类型说明符 (推荐)
//   对纯类型名使用 (cast 表达式、new 表达式、模板参数等)
//   parseType() == parseTypeSpecifier() + CVR 限定符
//
// 选项 B: 保留 parseType() 的旧行为
//   不推荐，因为会导致 parseDeclarator() 被调用两次
```
**采用选项 A**: `parseType()` 改为 `parseTypeSpecifier()` 的语义 — 仅解析类型说明符部分。
所有需要完整声明符的调用点 (parseDeclaration, parseClassMember, parseForStatement 等)
已通过 Step 7-12 迁移到 `parseDeclSpecifierSeq()` + `parseDeclarator(Declarator&)`。
不再需要调用 `parseType()` 内部的旧 `parseDeclarator(QualType)`。

#### 14.3 清理 Parser.h
```cpp
// 删除旧声明:
// QualType parseDeclarator(QualType Base);          // Step 6 旧重载
// VarDecl *parseVariableDeclaration(QualType, ...); // Step 8 旧函数
// FunctionDecl *parseFunctionDeclaration(QualType, ...); // Step 8 旧函数

// 新增声明保留:
// void parseDeclSpecifierSeq(DeclSpec &DS);
// void parseDeclarator(Declarator &D);
// VarDecl *buildVarDecl(Declarator &D);
// FunctionDecl *buildFunctionDecl(Declarator &D);
```

#### 14.4 清理 ParseType.cpp
- 删除 `parseDeclarator(QualType Base)` 旧实现 (约 60 行)
- `parseType()` 内部移除 `Result = parseDeclarator(Result)` 调用
- `parseFunctionDeclarator(QualType)` 保留 — 被新 `parseDeclarator(Declarator&)` 内部调用
- `parseArrayDimension(QualType)` 保留 — 被新 `parseDeclarator(Declarator&)` 内部调用
- 或将这些辅助函数变为 private 实现细节

#### 14.5 完全迁移验证
- `grep -rn "parseDeclarator(QualType" src/` → 零结果
- `grep -rn "parseVariableDeclaration(" src/` → 零结果 (旧签名)
- `grep -rn "parseFunctionDeclaration(" src/` → 零结果 (旧签名)
- 全部测试通过
- 编译零警告

### Step 15: 更新文档
- 更新 `PHASE3-GAP-ANALYSIS.md` 标记 Stage 3.2.1 和 3.2.2 均为 ✅
- 删除重构方案文档中的 "兼容性策略" 部分 (已无旧代码需兼容)

---

## 四、完全迁移路径总览

### 4.1 旧代码调用点清单 (必须全部迁移)

| # | 调用点 | 文件:行 | 旧模式 | 迁移目标 |
|---|--------|---------|--------|---------|
| 1 | `parseDeclaration()` 一般声明 | ParseDecl.cpp:270-336 | `bool IsStatic/Constexpr/Inline + parseType() + identifier + if/else` | DeclSpec + Declarator |
| 2 | `parseClassMember()` 成员声明 | ParseDecl.cpp:810-970 | `bool IsStatic/Mutable/Virtual + parseType() + identifier + 手动参数解析` | DeclSpec + Declarator(MemberContext) |
| 3 | `parseTypedefDeclaration()` | ParseDecl.cpp:2694-2720 | `parseType() + parseDeclarator(Type) + identifier` | DeclSpec + Declarator |
| 4 | `parseNonTypeTemplateParameter()` | ParseDecl.cpp:1580-1630 | `parseType() + parseDeclarator(Type) + identifier` | DeclSpec + Declarator(TemplateParamContext) |
| 5 | `parseParameterDeclaration()` | ParseDecl.cpp:461-488 | `parseType() + identifier + = default` | DeclSpec + Declarator(ParameterContext) |
| 6 | `parseForStatement()` | ParseStmt.cpp:581,600,711 | `parseType() + identifier` | DeclSpec + Declarator(ConditionContext) |
| 7 | `parseType()` 内部 | ParseType.cpp:33-43 | `parseTypeSpecifier() + parseDeclarator(Result)` | 仅保留 `parseTypeSpecifier()` |

### 4.2 不迁移的 parseType() 调用 (纯类型名使用)
以下调用点仅需要类型名 (不需要声明符/名称)，`parseType()` 保留为 `parseTypeSpecifier()`:
- cast 表达式: `parseCXXStaticCastExpr()` 等 → `CastType = parseType()` ✅
- new 表达式: `parseCXXNewExpression()` → `Type = parseType()` ✅
- catch 参数: `parseCXXCatchClause()` → `ExceptionType = parseType()` ✅
- 模板参数: `parseTemplateSpecializationType()` → `ArgType = parseType()` ✅
- 模板类型参数默认值: `parseTemplateTypeParameter()` → `DefaultType = parseType()` ✅
- trailing return type: `parseTrailingReturnType()` → `parseType()` ✅
- 基类: `parseBaseSpecifier()` → `BaseType = parseType()` ✅
- type alias: `using X = parseType()` ✅
- 函数参数类型: `parseFunctionDeclarator()` → `ParamType = parseType()` ✅

### 4.3 构建系统
新增源文件 `src/Parse/ParseDeclSpec.cpp` 需加入 `src/Parse/CMakeLists.txt`。

---

## 五、风险与缓解

| 风险 | 缓解措施 |
|------|---------|
| parseDeclarator 新旧重载共存导致混淆 | 新重载命名为 `parseDeclaratorChunked()` 直到旧版完全移除 |
| DeclaratorChunk::Function 需存储参数名 | 参数信息存储在 FunctionInfo 中，包含 ParmVarDecl 列表 |
| 多声明符 `int a, *b` 的 parseDeclaration 循环 | 在 `parseDeclaration()` 中添加循环，每次 parseDeclarator 前重新消费分隔符 |
| 回归风险 | 每步完成后运行全部测试 |
| parseClassMember 迁移工作量大 | 该函数约 200 行，逐段迁移 |

---

## 六、文件清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/Parse/DeclSpec.h` | **新建** | DeclSpec + StorageClass |
| `include/blocktype/Parse/DeclaratorChunk.h` | **新建** | DeclaratorChunk (5种Kind) |
| `include/blocktype/Parse/Declarator.h` | **新建** | DeclarationName + Declarator |
| `include/blocktype/Parse/Parser.h` | **修改** | 添加新函数声明，最终删除旧函数声明 |
| `src/Parse/ParseDeclSpec.cpp` | **新建** | parseDeclSpecifierSeq() 实现 |
| `src/Parse/ParseType.cpp` | **修改** | 新增 parseDeclarator(Declarator&)，最终删除旧 parseDeclarator(QualType) 和 parseType 中的旧调用 |
| `src/Parse/ParseDecl.cpp` | **修改** | 重构 parseDeclaration/parseClassMember/parseTypedefDeclaration/parseNonTypeTemplateParameter/parseParameterDeclaration，最终删除旧 parseVariableDeclaration/parseFunctionDeclaration |
| `src/Parse/ParseStmt.cpp` | **修改** | 迁移 parseForStatement 中的声明 |
| `src/Parse/CMakeLists.txt` | **修改** | 添加 ParseDeclSpec.cpp |
| `docs/PHASE3-GAP-ANALYSIS.md` | **修改** | 更新状态 |
