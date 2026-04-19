---
name: initlist-expected-type-infra
overview: 铺设 InitListExpr ExpectedType 完整基础设施：方案 B（Sema 后处理递归推导嵌套类型）+ 方案 A 预置架构（Parser 签名链 + deduceSubElementType 辅助函数），确保后续 narrowing check / initializer_list 歧义消解等功能可直接使用。
todos:
  - id: update-parser-h
    content: 更新 Parser.h 签名：parseInitializerClause/parseDesignatedInitializer 加 ExpectedType 参数，新增 deduceSubElementType 声明
    status: completed
  - id: implement-parser-chain
    content: 实现 ParseExpr.cpp 全链路：deduceSubElementType + parseInitializerList 循环传类型 + parseInitializerClause 转发 + parseDesignatedInitializer 字段查找 + ActOnInitListExpr 传入 ExpectedType
    status: completed
    dependencies:
      - update-parser-h
  - id: implement-sema-propagate
    content: 实现 Sema 安全网：Sema.h 声明 propagateTypesToNestedInitLists + Sema.cpp 实现递归类型传播 + ActOnInitListExpr 调用
    status: completed
    dependencies:
      - implement-parser-chain
  - id: build-test
    content: 编译并运行全部测试验证
    status: completed
    dependencies:
      - implement-sema-propagate
---

## 产品概述

为 BlockType 编译器的初始化列表类型推导系统建立完整的双层架构：Sema 后处理递归类型传播（方案 B，当前激活）+ Parser 签名链与类型拆解辅助函数（方案 A，预置架构），确保嵌套 `InitListExpr` 在所有已知类型的上下文中都能获得正确的类型信息。

## 核心功能

1. **Parser 层预置架构**：`parseInitializerClause` 和 `parseDesignatedInitializer` 增加 `QualType ExpectedType` 参数；新增 `deduceSubElementType` 辅助函数；`parseInitializerList` 循环中为每个子句计算并传递期望类型；将 `ExpectedType` 转发给 `ActOnInitListExpr`
2. **Sema 层安全网**：`ActOnInitListExpr` 中新增 `propagateTypesToNestedInitLists` 递归方法，对 Plan A 链路覆盖不到的场景（如 `parseAssignmentExpression` 无类型入口）做兜底类型传播
3. **指定初始化器类型查找**：`parseDesignatedInitializer` 利用 `ExpectedType`（RecordType）查找字段类型，传递给嵌套的 `parseInitializerClause`

## 技术方案

### 整体架构

双层类型传播机制，Plan A（Parser 期）覆盖已知类型路径，Plan B（Sema 期）作为安全网覆盖其余路径：

```
Plan A（Parser 期，主路径 — 有 ExpectedType 时激活）
═════════════════════════════════════════════════════
parseVarDecl(T) / parseCXXNewExpr(Type)
  └→ parseInitializerList(ExpectedType=T)
       loop: deduceSubElementType(T, i) → ElemType
            └→ parseInitializerClause(ElemType)
                 ├─ l_brace → parseInitializerList(ElemType)  ← 递归传播
                 ├─ .field  → parseDesignatedInitializer(AggrType)
                                └→ lookupField(AggrType, name) → FieldType
                                      └→ parseInitializerClause(FieldType)
                 └─ else → parseAssignmentExpression()
       └→ ActOnInitListExpr(LBraceLoc, Inits, RBraceLoc, ExpectedType)

parseAssignmentExpression()  ← 无类型入口
  └→ parseInitializerList()  ← ExpectedType=QualType()
       └→ ActOnInitListExpr(..., QualType())

Plan B（Sema 期，安全网 — 始终激活）
═══════════════════════════════════════
ActOnInitListExpr(..., ExpectedType)
  ├─ ILE->setType(ExpectedType)
  └─ propagateTypesToNestedInitLists(ILE, ExpectedType)
       └→ 遍历子 InitListExpr：
            仅当子节点 type 为空时：
              deduceElementType(ExpectedType, index) → setType
```

### deduceSubElementType 实现

核心辅助函数，负责将聚合类型拆解为第 i 个子元素的类型：

```
输入: QualType AggrType, unsigned Index
处理:
  1. 剥离包装层（循环）：
     - ElaboratedType → getNamedType()
     - TypedefType → getDecl()->getUnderlyingType().getTypePtr()
     - ReferenceType → getReferencedType()
     - QualType qualifiers → 保留到返回值
  2. 分解：
     - ArrayType → getElementType()（所有元素同类型）
     - RecordType → getDecl()->fields()[Index]->getType()
  3. 无法分解（标量/空等）→ 返回 QualType()
输出: QualType（子元素期望类型，可能为空）
```

### propagateTypesToNestedInitLists 实现

Sema 安全网，在 `ActOnInitListExpr` 内部调用：

```
输入: InitListExpr *ILE, QualType ExpectedType
处理:
  if (ExpectedType.isNull()) return;
  unsigned Index = 0;
  for (Expr *Init : ILE->getInits()):
    if (auto *NestedILE = dyn_cast<InitListExpr>(Init)):
      if (NestedILE->getType().isNull()):  ← 仅补漏
        QualType ElemType = deduceElementType(ExpectedType, Index)
        if (!ElemType.isNull()):
          NestedILE->setType(ElemType)
          propagateTypesToNestedInitLists(NestedILE, ElemType)  ← 递归
    Index++;
```

### 各函数签名变更

| 函数 | 当前签名 | 变更后签名 |
| --- | --- | --- |
| `parseInitializerClause` | `()` | `(QualType ExpectedType = QualType())` |
| `parseDesignatedInitializer` | `()` | `(QualType ExpectedType = QualType())` |
| `parseInitializerList` | `(QualType ExpectedType = QualType())` | 不变（已有参数） |
| `deduceSubElementType` | 不存在 | `(QualType AggrType, unsigned Index) → QualType` [新增] |
| `propagateTypesToNestedInitLists` | 不存在 | Sema 私有方法 [新增] |


### parseInitializerList 循环修改

当前代码（第 1349-1351 行）：

```cpp
while (!Tok.is(TokenKind::r_brace) && !Tok.is(TokenKind::eof)) {
    Expr *Init = parseInitializerClause();
```

修改后：

```cpp
unsigned ElemIndex = 0;
while (!Tok.is(TokenKind::r_brace) && !Tok.is(TokenKind::eof)) {
    QualType ElemType = deduceSubElementType(ExpectedType, ElemIndex);
    Expr *Init = parseInitializerClause(ElemType);
```

`ElemIndex` 在每次成功解析后递增。

### parseInitializerClause 修改

当前代码（第 1391-1404 行）：

```cpp
Expr *Parser::parseInitializerClause() {
  if (Tok.is(TokenKind::period))
    return parseDesignatedInitializer();
  if (Tok.is(TokenKind::l_brace))
    return parseInitializerList();
  return parseAssignmentExpression();
}
```

修改后：

```cpp
Expr *Parser::parseInitializerClause(QualType ExpectedType) {
  if (Tok.is(TokenKind::period))
    return parseDesignatedInitializer(ExpectedType);
  if (Tok.is(TokenKind::l_brace))
    return parseInitializerList(ExpectedType);
  return parseAssignmentExpression();
}
```

### parseDesignatedInitializer 修改

当前代码第 1438 行 `parseInitializerClause()` 改为：

```cpp
Expr *Parser::parseDesignatedInitializer(QualType ExpectedType) {
  // ... 解析 .field_name = ...
  
  // 从 ExpectedType（RecordType）查找字段类型
  QualType FieldType;
  if (!ExpectedType.isNull()) {
    if (auto *RT = dyn_cast<RecordType>(ExpectedType.getTypePtr())) {
      for (auto *F : RT->getDecl()->fields()) {
        if (F->getName() == FieldName) {
          FieldType = F->getType();
          break;
        }
      }
    }
  }
  
  Expr *Init = parseInitializerClause(FieldType);
  // ...
}
```

### parseInitializerList 尾部修改

当前第 1383 行：

```cpp
return Actions.ActOnInitListExpr(LBraceLoc, Inits, RBraceLoc).get();
```

修改为：

```cpp
return Actions.ActOnInitListExpr(LBraceLoc, Inits, RBraceLoc, ExpectedType).get();
```

### 向后兼容性

- 所有新增参数都有默认值 `QualType()`，现有调用者无需修改
- `parseAssignmentExpression` 中 `parseInitializerList()` 调用保持不变（默认空类型）
- `parseVarDecl` 和 `parseCXXNewExpression` 已传类型，无需修改
- Plan B 的 `propagateTypesToNestedInitLists` 仅在子节点类型为空时生效，不覆盖 Plan A 已设的值

## 文件变更

```
project-root/
├── include/blocktype/Parse/Parser.h
│   [MODIFY] 更新函数签名
│   - parseInitializerClause 加 QualType ExpectedType = QualType() 参数
│   - parseDesignatedInitializer 加 QualType ExpectedType = QualType() 参数
│   - 新增 deduceSubElementType(QualType, unsigned) 私有方法声明
│
├── src/Parse/ParseExpr.cpp
│   [MODIFY] Parser 链路实现（核心变更）
│   - 新增 deduceSubElementType 实现（~20 行）
│   - parseInitializerList: 循环中加 ElemIndex 计数，传 ElemType 给 parseInitializerClause，转发 ExpectedType 给 ActOnInitListExpr
│   - parseInitializerClause: 接收 ExpectedType，转发给 parseInitializerList / parseDesignatedInitializer
│   - parseDesignatedInitializer: 接收 ExpectedType，查找字段类型，传给嵌套 parseInitializerClause
│
├── include/blocktype/Sema/Sema.h
│   [MODIFY] 新增 propagateTypesToNestedInitLists 私有方法声明
│
└── src/Sema/Sema.cpp
    [MODIFY] Sema 安全网实现
    - 新增 propagateTypesToNestedInitLists 实现（~25 行）
    - ActOnInitListExpr 中调用 propagateTypesToNestedInitLists
    - deduceElementType 辅助函数（与 Parser 版逻辑一致，用于 Sema 侧）
```