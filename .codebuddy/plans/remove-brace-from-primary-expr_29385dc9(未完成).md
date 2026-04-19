---
name: remove-brace-from-primary-expr
overview: 将 parsePrimaryExpression 中 l_brace 的处理移除，改为在各合法语法位置（函数参数、return、初始化器等）显式解析 braced-init-list
todos:
  - id: move-brace-to-assign
    content: parsePrimaryExpression 删除 l_brace case + parseAssignmentExpression 添加花括号拦截
    status: pending
  - id: build-test
    content: 编译 + 662 测试验证
    status: pending
    dependencies:
      - move-brace-to-assign
---

## 问题

`parsePrimaryExpression` 中 `case TokenKind::l_brace: return parseInitializerList()` 是前期简化实现。在 C++ 标准中，braced-init-list **不是** primary-expression，不能出现在 `x + {1,2}`、`*{1,2}` 等位置。

## 目标

将花括号列表的处理从 `parsePrimaryExpression` 移到 `parseAssignmentExpression`，使其只能出现在合法的语法位置（赋值右侧、函数参数、return 等），与 Clang 实现一致。

## 核心修改

1. 从 `parsePrimaryExpression` 移除 `l_brace` case
2. 在 `parseAssignmentExpression` 开头增加 `{` 检查，匹配时直接返回 `parseInitializerList()`

## 技术分析

### 当前调用链

```
parseExpression()
  → parseUnaryExpression()
    → parsePrimaryExpression()   ← l_brace 在这里（错误！）
      → parseInitializerList()
  → parseRHS(Comma)

parseAssignmentExpression()
  → parseUnaryExpression()
    → parsePrimaryExpression()   ← l_brace 在这里（错误！）
  → parseRHS(Assignment)
```

### 正确的调用链（Clang 做法）

```
parseAssignmentExpression()
  if (Tok == l_brace)
    → parseInitializerList()     ← 在这里拦截（正确位置）
  else
  → parseUnaryExpression()
    → parsePrimaryExpression()   ← l_brace 不再出现
  → parseRHS(Assignment)
```

### 为什么 `parseAssignmentExpression` 是正确位置

C++ 标准 [expr.ass] 允许 braced-init-list 出现在 `assignment-expression` 的位置：

- `x = {1,2}` — 赋值右侧
- `f({1,2})` — 函数参数（通过 `parseCallArguments` → `parseAssignmentExpression`）
- `return {1,2}` — return 表达式（通过 `parseExpression` → `parseAssignmentExpression`）
- `arr[{1}]` — 下标（通过 subscript → `parseAssignmentExpression`）

而以下路径经过 `parseUnaryExpression` → `parsePrimaryExpression`，**不应**遇到花括号：

- `*{1,2}` — 解引用花括号列表（非法）
- `x + {1,2}` — 二元运算（非法）
- `sizeof {1,2}` — sizeof 操作数（有歧义，但不是 primary）

## 修改方案

### 修改 1：`parseAssignmentExpression`（ParseExpr.cpp:233-246）

在调用 `parseUnaryExpression()` 之前，增加花括号检查：

```cpp
Expr *Parser::parseAssignmentExpression() {
  pushContext(ParsingContext::Expression);

  // C++11: braced-init-list can appear as an assignment-expression.
  // This is the correct syntactic position (not primary-expression).
  if (Tok.is(TokenKind::l_brace)) {
    Expr *Result = parseInitializerList();
    popContext();
    return Result;
  }

  Expr *LHS = parseUnaryExpression();
  if (!LHS) {
    popContext();
    return nullptr;
  }

  Expr *Result = parseRHS(LHS, PrecedenceLevel::Assignment);
  popContext();
  return Result;
}
```

### 修改 2：`parsePrimaryExpression`（ParseExpr.cpp:624-626）

删除花括号 case：

```cpp
  // 删除这 3 行：
  // // Brace-enclosed initializer list
  // case TokenKind::l_brace:
  //   return parseInitializerList();
```

### 修改 3：`parseExpressionWithPrecedence`（ParseExpr.cpp:248-254）

此函数也直接调用 `parseUnaryExpression`，如果它在花括号合法的上下文中被调用也需要处理。但检查其调用者——它是 `parseExpression` 内部使用的，花括号在 `parseAssignmentExpression` 已拦截，所以不需要额外修改。

## 影响范围

所有花括号列表的合法使用路径仍然工作：

| 用法 | 调用路径 | 是否经过 parseAssignmentExpression |
| --- | --- | --- |
| `int x = {1,2}` | buildVarDecl 直接调用 parseInitializerList | N/A（已独立处理） |
| `new T{1,2}` | parseCXXNewExpression 直接调用 parseInitializerList | N/A（已独立处理） |
| `f({1,2})` | parseCallArguments → parseAssignmentExpression | 是 |
| `return {1,2}` | parseExpression → parseAssignmentExpression | 是 |
| `x = {1,2}` | parseRHS → parseAssignmentExpression | 是 |
| `arr[{1}]` | subscript → parseAssignmentExpression | 是 |


## 文件变更

- `src/Parse/ParseExpr.cpp`
- `parsePrimaryExpression`：删除 `l_brace` case（3 行）
- `parseAssignmentExpression`：在 `parseUnaryExpression()` 前加花括号检查（5 行）