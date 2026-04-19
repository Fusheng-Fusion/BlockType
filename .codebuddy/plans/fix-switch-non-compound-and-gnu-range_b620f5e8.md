---
name: fix-switch-non-compound-and-gnu-range
overview: 修复 SwitchStmt 的两个 P2 问题：(1) 非 CompoundStmt switch body 支持和 (2) GNU case range 代码生成
todos:
  - id: parse-gnu-range
    content: Parse 层：parseCaseStatement 检测 ellipsis token 解析 RHS 表达式
    status: completed
  - id: sema-case-rhs
    content: Sema 层：ActOnCaseStmt 增加 RHS 参数，传入 CaseStmt 构造
    status: completed
    dependencies:
      - parse-gnu-range
  - id: codegen-non-compound
    content: CodeGen 层：重构 CollectCases 和 EmitSwitchBody 为递归遍历，统一处理 CompoundStmt 和非 CompoundStmt body
    status: completed
  - id: codegen-gnu-range
    content: CodeGen 层：CaseInfo 增加 range 字段，生成 case range 条件判断代码
    status: completed
    dependencies:
      - codegen-non-compound
  - id: build-test
    content: 编译并运行全部测试验证无回归
    status: completed
    dependencies:
      - sema-case-rhs
      - codegen-gnu-range
---

## 产品概述

根源性修复 BlockType 编译器 `EmitSwitchStmt` 中两个 P2 遗留问题：非 CompoundStmt switch body 处理和 GNU case range (`case 1...5:`) 代码生成，实现与 Clang 对齐的完整 switch 语句支持。

## 核心功能

1. **非 CompoundStmt switch body 完整处理**：当 switch body 不是 `{...}` 而是 `case 1: ... case 2: ...` 链式结构时，通过递归遍历 `CaseStmt → SubStmt → CaseStmt` 链来收集和生成所有 case 分支，覆盖 `switch(x) case 1: foo();` 和 `switch(x) default: bar();` 等合法写法
2. **GNU case range (`case L...R:`) 完整支持**：从 Parse 层解析 `...` token 构造带 RHS 的 CaseStmt，到 CodeGen 层在 default 分支中生成 `LHS <= cond && cond <= RHS` 的范围判断条件跳转，实现 `case 'a'...'z':` 等 GNU 扩展语法

## 技术方案

### 问题分析

#### 问题 3：非 CompoundStmt switch body

**根因**：C/C++ 允许 `switch(x) case 1: foo();` 写法。此时 Parse 层的 `parseStatement()` 返回单个 `CaseStmt`，而该 CaseStmt 的 `SubStmt` 可能是下一个 CaseStmt（形成链）。当前 `CollectCases` 和 `EmitSwitchBody` 对非 CompoundStmt body 只检查顶层 CaseStmt/DefaultStmt，不递归遍历 SubStmt 中的嵌套 CaseStmt 链。

**Parse 层实际行为**：

```
switch(x) case 1: case 2: foo(); break;
```

解析为：

```
SwitchStmt
  Cond: x
  Body: CaseStmt(1, SubStmt=CaseStmt(2, SubStmt=ExprStmt(foo(); )))
```

即 CaseStmt 的 SubStmt 不是普通语句，而是下一个 CaseStmt。

**方案**：抽取 `collectCasesFromStmt` 递归辅助函数，统一处理 CompoundStmt 和非 CompoundStmt body。对每个 stmt 检查是否为 CaseStmt/DefaultStmt，若是 CaseStmt 则收集其值后递归处理 SubStmt 中的嵌套 CaseStmt/DefaultStmt。这样 CompoundStmt 和非 CompoundStmt 走同一条代码路径。

#### 问题 4：GNU case range

**根因**：`CaseStmt` 已有 `RHS` 字段，但 Parse 层不解析 `...`，Sema 层传 `nullptr`，CodeGen 层不生成范围判断代码。

**Clang 做法**：

1. Parse 层检测 `...` token 解析 RHS 表达式
2. CodeGen 层将普通 case 值加入 `SwitchInst`，将 case range 的判断放在 default 分支中：生成 `cond >= LHS && cond <= RHS` 的条件判断，跳转到对应 case BB

**方案**（三层修改）：

**Parse 层**：`parseCaseStatement()` 中，解析完 LHS 后检测 `TokenKind::ellipsis`，若存在则解析 RHS 表达式

**Sema 层**：`ActOnCaseStmt` 增加 `Expr *RHS` 参数，传入 CaseStmt 构造

**CodeGen 层**：

1. `CaseInfo` 结构体增加 `bool IsRange` 和 `llvm::ConstantInt *UpperValue` 字段
2. `CollectCases` 对有 RHS 的 CaseStmt 记录为 range case（不加入 SwitchInst）
3. `EmitSwitchStmt` 创建 SwitchInst 时排除 range case
4. 在 default 分支之前插入范围判断代码：遍历所有 range case，生成 `cond >= lower && cond <= upper` 的条件链，匹配则跳转到对应 BB
5. 范围判断不匹配时 fall through 到原始 default BB

### 实现细节

#### 非 CompoundStmt 处理 — 重构为统一递归遍历

替换 `CollectCases` lambda 中的 CompoundStmt/非CompoundStmt 分支为统一的递归函数：

```cpp
// 从单个 stmt 中递归收集 case/default 信息
auto collectCasesFromStmt = [&](Stmt *S) {
  while (S) {
    if (auto *CaseS = llvm::dyn_cast<CaseStmt>(S)) {
      // 收集 LHS 值（普通 case 或 range case 的下界）
      if (auto *CaseExpr = CaseS->getLHS()) {
        llvm::Constant *ConstVal = CGM.getConstants().EmitConstant(CaseExpr);
        if (auto *CI = llvm::dyn_cast_or_null<llvm::ConstantInt>(ConstVal)) {
          llvm::BasicBlock *CaseBB = createBasicBlock("switch.case");
          bool IsRange = (CaseS->getRHS() != nullptr);
          llvm::ConstantInt *UpperCI = nullptr;
          if (IsRange) {
            llvm::Constant *UpperVal = CGM.getConstants().EmitConstant(CaseS->getRHS());
            UpperCI = llvm::dyn_cast_or_null<llvm::ConstantInt>(UpperVal);
          }
          Cases.push_back({CI, UpperCI, CaseBB, IsRange});
          CaseValueToBB[CI] = CaseBB;
        }
      }
      S = CaseS->getSubStmt(); // 递归进入 SubStmt（可能是下一个 CaseStmt）
    } else if (auto *DefaultS = llvm::dyn_cast<DefaultStmt>(S)) {
      FoundDefaultBB = DefaultBB;
      S = DefaultS->getSubStmt(); // DefaultStmt 的 SubStmt 也可能包含更多 case
    } else {
      break; // 普通语句，停止
    }
  }
};
```

对外层调用统一处理：

```cpp
Stmt *Body = SwitchStatement->getBody();
if (auto *CS = llvm::dyn_cast<CompoundStmt>(Body)) {
  for (Stmt *S : CS->getBody())
    collectCasesFromStmt(S);
} else {
  collectCasesFromStmt(Body);
}
```

`EmitSwitchBody` lambda 同样重构为统一的递归遍历。

#### GNU case range — 代码生成

在 `SwitchInst` 创建后、生成 case body 之前，为 range case 生成范围判断：

```cpp
// 如果有 range case，在 default 路径中插入范围判断
if (hasRangeCases) {
  llvm::BasicBlock *RangeCheckBB = createBasicBlock("switch.range");
  // 修改 SwitchInst 的 default 为 RangeCheckBB
  SwitchInst->setDefaultDest(RangeCheckBB);
  EmitBlock(RangeCheckBB);
  
  for (auto &RC : RangeCases) {
    llvm::BasicBlock *NextBB = (next range check BB or DefaultBB);
    llvm::Value *LowerCmp = Builder.CreateICmpSGE(Cond, RC.Lower, "range.lb");
    llvm::Value *UpperCmp = Builder.CreateICmpSLE(Cond, RC.Upper, "range.ub");
    llvm::Value *InRange = Builder.CreateAnd(LowerCmp, UpperCmp, "range.in");
    Builder.CreateCondBr(InRange, RC.BB, NextBB);
    EmitBlock(NextBB);
  }
  // 最终 fall through 到 DefaultBB
  Builder.CreateBr(DefaultBB);
}
```

#### Parse 层 — `parseCaseStatement` 添加 ellipsis 检测

```cpp
Expr *RHS = nullptr;
if (Tok.is(TokenKind::ellipsis)) {
  consumeToken(); // consume '...'
  RHS = parseExpression();
}
```

#### Sema 层 — `ActOnCaseStmt` 增加 RHS 参数

```cpp
StmtResult ActOnCaseStmt(Expr *Val, Expr *RHS, Stmt *Body, SourceLocation CaseLoc);
```

## 文件变更

```
project-root/
├── src/Parse/ParseStmt.cpp
│   [MODIFY] parseCaseStatement: 检测 ellipsis token 解析 GNU case range RHS
│
├── include/blocktype/Sema/Sema.h
│   [MODIFY] ActOnCaseStmt 签名增加 Expr *RHS 参数
│
├── src/Sema/Sema.cpp
│   [MODIFY] ActOnCaseStmt: 接收 RHS 参数，传入 CaseStmt 构造
│
├── src/CodeGen/CodeGenStmt.cpp
│   [MODIFY] EmitSwitchStmt 完整重构：
│   - CaseInfo 增加 IsRange + UpperValue 字段
│   - CollectCases 重构为递归 collectCasesFromStmt 统一处理链式和 CompoundStmt
│   - EmitSwitchBody 同样重构为递归遍历
│   - 新增 range case 条件判断代码生成（在 default 路径中）
│   - 非 CompoundStmt body 通过递归遍历 CaseStmt 链完整处理
```