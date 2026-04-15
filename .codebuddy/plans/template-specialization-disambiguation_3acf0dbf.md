---
name: template-specialization-disambiguation
overview: 实现模板特化与比较表达式的混合判断方案：符号表查找 + 试探性解析，解决 Vector<T> 被误识别为比较表达式的问题
todos:
  - id: add-preprocessor-state-api
    content: 在 Preprocessor 中添加 saveTokenBufferState/restoreTokenBufferState/clearTokenBuffer 方法
    status: completed
  - id: implement-tentative-parsing
    content: 实现 TentativeParsingAction 类，支持保存和恢复解析状态
    status: completed
    dependencies:
      - add-preprocessor-state-api
  - id: refactor-parse-identifier
    content: 重构 parseIdentifier 和 parseQualifiedName，实现三层判断策略
    status: completed
    dependencies:
      - implement-tentative-parsing
  - id: add-template-tests
    content: 添加模板特化表达式测试用例（Vector<T>, Container<U>等）
    status: completed
    dependencies:
      - refactor-parse-identifier
  - id: run-all-tests
    content: 运行全部368个测试确保无回归
    status: completed
    dependencies:
      - add-template-tests
---

## 用户需求

修复技术债务：模板特化与比较表达式的启发式判断问题（TECHNICAL_DEBT_INVENTORY.md 第2项）

## 问题概述

当前 `parseIdentifier()` 和 `parseQualifiedName()` 使用简单启发式方法区分模板特化和比较表达式，只检查 `<` 后是否为类型关键字（int, float等），导致 `Vector<T>`、`Container<U>` 等模板特化表达式被误识别为比较表达式。

## 核心功能

1. 实现符号表查找：检查标识符是否为已知的 TemplateDecl
2. 实现试探性解析机制：当符号表查找失败时，尝试解析模板参数列表
3. 实现回溯机制：解析失败时能够回退到原始状态
4. 添加测试用例验证修复效果

## 视觉效果

无（这是编译器内部修复，不涉及UI）

## 技术栈

- 语言：C++17
- 依赖：LLVM Support Library
- 架构模式：现有 Parser/Preprocessor 架构扩展

## 实现方案：混合方案（符号表查找 + 试探性解析）

### 核心设计

采用三层判断策略：

1. **第一层**：检查 `<` 后是否为类型关键字（保留现有逻辑）
2. **第二层**：符号表查找，检查标识符是否为已知 TemplateDecl
3. **第三层**：试探性解析模板参数列表，验证是否合法结束

### 试探性解析机制

```
TentativeParsingAction:
├── 保存 Preprocessor::TokenBufferIndex
├── 保存 Parser::Tok 和 Parser::NextTok
├── 尝试解析模板参数
├── 检查结束条件（>, >>, >>等）
└── Commit 或 Abort 恢复状态
```

### 性能考虑

- 符号表查找 O(1)（StringMap），优先使用
- 试探性解析仅在前两层失败时触发
- TokenBuffer 已有机制，无需额外内存分配

## 目录结构

```
project-root/
├── include/blocktype/
│   ├── Lex/
│   │   └── Preprocessor.h       # [MODIFY] 添加 saveTokenBuffer/restoreTokenBuffer 方法
│   └── Parse/
│       └── Parser.h              # [MODIFY] 添加 TentativeParsingAction 类声明
├── src/
│   ├── Lex/
│   │   └── Preprocessor.cpp      # [MODIFY] 实现 token buffer 保存/恢复
│   └── Parse/
│       └── ParseExpr.cpp         # [MODIFY] 重构 parseIdentifier 和 parseQualifiedName
└── tests/
    └── unit/Parse/
        └── ParserTest.cpp        # [MODIFY] 添加模板特化表达式测试
```

## 关键代码结构

```cpp
// Preprocessor.h - Token buffer 状态保存接口
class Preprocessor {
public:
  /// 保存当前 token buffer 状态
  size_t saveTokenBufferState() const { return TokenBufferIndex; }
  
  /// 恢复 token buffer 状态
  void restoreTokenBufferState(size_t State);
  
  /// 清除 token buffer（用于 commit）
  void clearTokenBuffer();
};

// Parser.h - 试探性解析动作类
class TentativeParsingAction {
  Parser &P;
  Token SavedTok, SavedNextTok;
  size_t SavedBufferIndex;
  bool Committed = false;
public:
  explicit TentativeParsingAction(Parser &Parser);
  ~TentativeParsingAction();
  Expr *commit(Expr *Result);  // 确认解析结果
  void abort();                 // 放弃并恢复状态
};
```

## 实现细节

### 判断逻辑流程

```cpp
Expr *Parser::parseIdentifier() {
  // ... 解析标识符 ...
  
  if (Tok.is(TokenKind::less)) {
    // 第一层：类型关键字判断（保留）
    if (isTypeKeyword(PP.peekToken(0))) {
      return parseTemplateSpecializationExpr(Loc, Name);
    }
    
    // 第二层：符号表查找
    if (CurrentScope) {
      if (NamedDecl *D = CurrentScope->lookup(Name)) {
        if (llvm::isa<TemplateDecl>(D)) {
          return parseTemplateSpecializationExpr(Loc, Name);
        }
      }
    }
    
    // 第三层：试探性解析
    return tryParseTemplateOrComparison(Loc, Name);
  }
  // ...
}

Expr *Parser::tryParseTemplateOrComparison(SourceLocation Loc, StringRef Name) {
  TentativeParsingAction Action(*this);
  
  // 尝试解析模板参数列表
  consumeToken(); // <
  llvm::SmallVector<TemplateArgument, 4> Args;
  bool IsValidTemplate = tryParseTemplateArguments(Args);
  
  // 检查结束条件
  if (IsValidTemplate && Tok.is(TokenKind::greater)) {
    Action.commit();
    return Context.create<TemplateSpecializationExpr>(Loc, Name, Args);
  }
  
  // 回退，解析为比较表达式
  Action.abort();
  return parseComparisonExpr(Loc, Name);
}
```

## Agent Extensions

### SubAgent

- **code-explorer**
- Purpose: 深入探索 Parser 和 Preprocessor 的 token 管理机制，确保回溯实现正确
- Expected outcome: 确认 TokenBuffer 和 TokenBufferIndex 的完整生命周期管理