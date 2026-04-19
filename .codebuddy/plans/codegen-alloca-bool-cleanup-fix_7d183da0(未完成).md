---
name: codegen-alloca-bool-cleanup-fix
overview: 根源性解决 PHASE6-6.2-AUDIT 中 3 个 P2 遗留问题：AllocaInsertPt 优化、条件表达式 bool 转换统一化、Cleanups 栈基础设施搭建。
todos:
  - id: alloca-insert-pt
    content: 实现 AllocaInsertPt：CodeGenFunction.h 新增成员 + EmitFunctionBody 初始化 + CreateAlloca 简化 + CreateEntryBlockAlloca 新方法
    status: pending
  - id: eliminate-manual-saveip
    content: 消除 5 处手动 saveIP/restoreIP：CodeGenExpr.cpp (2) + CodeGenStmt.cpp (3) 改用 CreateEntryBlockAlloca
    status: pending
    dependencies:
      - alloca-insert-pt
  - id: unify-bool-conversion
    content: 统一 5 处内联 CreateICmpNE 为 EmitConversionToBool 调用（EmitLogicalAnd/Or/ConditionalOperator）
    status: pending
  - id: cleanup-stack-infra
    content: 搭建 Cleanups 栈：CodeGenFunction.h 新增 CleanupEntry/RunCleanupsScope + CodeGenStmt.cpp 实现析构调用逻辑
    status: pending
  - id: cleanup-integration
    content: 集成 Cleanups：EmitDeclStmt 注册 + EmitCompoundStmt/控制流语句包裹 RunCleanupsScope + EmitReturnStmt 清理
    status: pending
    dependencies:
      - cleanup-stack-infra
  - id: build-test
    content: 编译并运行全部 662 测试验证无回归
    status: pending
    dependencies:
      - eliminate-manual-saveip
      - unify-bool-conversion
      - cleanup-integration
---

## 产品概述

根源性解决 BlockType 编译器 CodeGen 模块的三个 P2 遗留问题，提升代码质量与 Clang 架构对齐度。

## 核心功能

1. **AllocaInsertPt 成员变量**：在 `CodeGenFunction` 中引入 `AllocaInsertPt`，消除所有手动 `saveIP/restoreIP` 的重复代码模式，`CreateAlloca` 直接在固定插入点分配，提高 IR 生成效率和代码一致性
2. **统一 bool 转换**：将 `EmitLogicalAnd`、`EmitLogicalOr`、`EmitConditionalOperator` 中 5 处内联 `CreateICmpNE + getNullValue` 替换为 `EmitConversionToBool` 调用，消除代码重复和 i1 冗余比较问题
3. **Cleanups 栈基础设施**：新增 `EHScope` 栈和 `RunCleanupsScope` RAII 类，为作用域结束时的自动析构函数调用和异常处理 cleanup 提供基础框架

## 技术方案

### Issue 4: AllocaInsertPt — 消除 saveIP/restoreIP 重复模式

**根因**：每次创建 alloca 都重复 `saveIP → find entry block → set insert point → alloca → restoreIP` 模式（6 处手动 + CreateAlloca 内 1 处），效率低且代码冗余。

**Clang 做法**：在 `EmitFunctionBody` 创建 entry block 后，保存 `AllocaInsertPt = new IP`，之后所有 alloca 直接在该点插入，无需反复 save/restore。

**方案**：

1. `CodeGenFunction.h` 新增 `llvm::AllocaInst *AllocaInsertPt = nullptr` 成员变量
2. `EmitFunctionBody` 在创建 entry block 并完成参数 alloca 后，在参数 alloca 之后创建一个 `AllocaInsertPt` 位置的锚点（使用临时 dummy alloca 或直接用 `getFirstNonPHIOrDbg()` 的下一个位置）
3. `CreateAlloca` 改为直接在 `AllocaInsertPt` 位置插入，删除内部的 saveIP/restoreIP
4. 新增 `CreateEntryBlockAlloca(llvm::Type*, StringRef)` 私有方法，供手动 alloca 处使用
5. 5 处手动 saveIP/restoreIP 全部改为调用 `CreateEntryBlockAlloca` 或 `CreateAlloca`

**AllocaInsertPt 锚点实现**：在 `EmitFunctionBody` 参数 alloca 循环结束后，保存 `AllocaInsertPt = Builder.saveIP()`，后续所有 alloca 都在该点之前插入（通过 `AllocaInsertPt` 的 insertion point 指令之前 insert）。

### Issue 5: 统一 EmitConversionToBool

**根因**：`EmitConversionToBool` 已正确实现 i1/整数/浮点/指针分类处理，但 5 处调用仍用内联 `CreateICmpNE + getNullValue`（不区分类型，且对已是 i1 的值产生冗余比较）。

**方案**：

1. `EmitLogicalAnd`（第 119-121 行、131-133 行）替换为 `EmitConversionToBool(LHS, LHSExpr->getType())`
2. `EmitLogicalOr`（第 160-162 行、172-174 行）同上
3. `EmitConditionalOperator`（第 1163-1164 行）同上
4. 需要获取操作数的 AST 类型（通过 `BinaryOp->getLHS()->getType()` / `BinaryOp->getRHS()->getType()` / `Conditional->getCond()->getType()`）

**注意**：`EmitLogicalAnd/Or` 中 LHS 的 `EmitConversionToBool` 调用必须在 `getCurrentBlock()` 保存之后（因为 PHI 节点需要知道 LHS 所在的 BasicBlock），当前代码中 `LHBB = getCurrentBlock()` 已在 `CreateICmpNE` 之前，替换后仍需保持此顺序。但 `EmitConversionToBool` 不会改变当前 BasicBlock（它只生成计算指令，不生成终止指令），所以顺序安全。

### Issue 6: Cleanups 栈基础框架

**根因**：局部类类型变量在作用域结束时不会自动调用析构函数，且缺少异常处理 cleanup 基础设施。

**方案**：搭建最小可用的 cleanup 框架（不做完整 EH，仅做自动析构）：

1. **`CodeGenFunction.h`** 新增：

- `struct CleanupEntry { VarDecl *VD; CXXRecordDecl *RD; }` — 记录需要析构的变量
- `llvm::SmallVector<CleanupEntry, 8> CleanupStack` — cleanup 栈
- `void pushCleanup(VarDecl *VD)` — 注册需要析构的变量
- `void popCleanup()` — 弹出栈顶
- `void EmitCleanupsForScope(unsigned OldSize)` — 从栈顶到 OldSize 逆序调用析构

2. **`RunCleanupsScope`** RAII 类：

- 构造时记录 `CleanupStack.size()`
- 析构时调用 `EmitCleanupsForScope(SavedSize)`，逆序调用析构函数

3. **集成点**：

- `EmitDeclStmt`：如果变量类型有非 trivial 析构函数，`pushCleanup(VD)`
- `EmitCompoundStmt`：用 `RunCleanupsScope` 包裹语句序列，退出时自动析构
- `EmitIfStmt`/`EmitForStmt`/`EmitWhileStmt`/`EmitDoStmt`：then/else/body 块使用 `RunCleanupsScope`
- `EmitReturnStmt`：在跳转到 ReturnBlock 之前调用 `EmitCleanupsForScope(0)` 清理所有活跃变量

**析构函数调用**：复用已有的 `CGM.getCXX().EmitDestructorCall(*this, RD, AllocaPtr)` 接口。

## 文件变更

```
project-root/
├── include/blocktype/CodeGen/CodeGenFunction.h
│   [MODIFY] 新增成员和接口
│   - 新增 AllocaInsertPt 成员变量
│   - 新增 CreateEntryBlockAlloca(llvm::Type*, StringRef) 私有方法
│   - 新增 CleanupEntry 结构体 + CleanupStack 成员
│   - 新增 pushCleanup/popCleanup/EmitCleanupsForScope 方法
│   - 新增 RunCleanupsScope 嵌套类
│
├── src/CodeGen/CodeGenFunction.cpp
│   [MODIFY] AllocaInsertPt 初始化 + CreateAlloca 简化
│   - EmitFunctionBody: 参数 alloca 循环后保存 AllocaInsertPt
│   - CreateAlloca: 使用 AllocaInsertPt 直接插入，删除 saveIP/restoreIP
│   - CreateEntryBlockAlloca: 新方法，供手动 alloca 使用
│
├── src/CodeGen/CodeGenExpr.cpp
│   [MODIFY] 统一 bool 转换 + 消除手动 alloca
│   - EmitLogicalAnd: 2 处 CreateICmpNE → EmitConversionToBool（需传 AST 类型）
│   - EmitLogicalOr: 2 处 CreateICmpNE → EmitConversionToBool
│   - EmitConditionalOperator: 1 处 CreateICmpNE → EmitConversionToBool
│   - EmitCXXNewExpr: 数组构造循环计数器，手动 saveIP → CreateEntryBlockAlloca
│   - EmitCXXDeleteExpr: 数组析构循环计数器，手动 saveIP → CreateEntryBlockAlloca
│
├── src/CodeGen/CodeGenStmt.cpp
│   [MODIFY] 消除手动 alloca + 集成 Cleanups
│   - EmitCXXTryStmt: landingpad alloca，手动 saveIP → CreateEntryBlockAlloca
│   - EmitCXXForRangeStmt: 2 处迭代器 alloca，手动 saveIP → CreateEntryBlockAlloca
│   - EmitCompoundStmt: 添加 RunCleanupsScope 包裹
│   - EmitDeclStmt: 检测类类型变量有析构函数时 pushCleanup
│   - EmitReturnStmt: 跳转前调用 EmitCleanupsForScope(0)
│   - EmitIfStmt/EmitForStmt/EmitWhileStmt/EmitDoStmt: body 用 RunCleanupsScope
```