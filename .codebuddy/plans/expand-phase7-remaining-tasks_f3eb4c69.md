---
name: expand-phase7-remaining-tasks
overview: 继续扩展 Phase 7 文档：展开 cpp26-features.md 中的 Task 7.5.1 和 7.5.2，补充详细接口规划中 Task 7.5.1 的接口定义，同步验证报告和特性清单的状态。
todos:
  - id: expand-751
    content: 展开 07-PHASE7-cpp26-features.md Task 7.5.1 转义序列扩展
    status: completed
  - id: expand-752
    content: 展开 07-PHASE7-cpp26-features.md Task 7.5.2 八个 C++26 子特性
    status: completed
    dependencies:
      - expand-751
  - id: add-751-interface
    content: 补充 07-PHASE7-detailed-interface-plan.md Task 7.5.1 接口定义
    status: completed
    dependencies:
      - expand-751
  - id: sync-verification
    content: 更新 07-PHASE7-feature-verification-report.md
    status: completed
    dependencies:
      - expand-752
  - id: sync-features-status
    content: 同步 CPP23-CPP26-FEATURES.md 状态冲突
    status: completed
    dependencies:
      - sync-verification
---

## 用户需求

对 `docs/plan/` 中 Phase 7 开发文档进行扩展、细化和优化：

1. 优化功能描述完整性 — 对概括性内容细化，补充缺失内容
2. 优化接口描述完整性 — 每个模块的接口（头文件声明）必须完整

## 待完成工作

基于上一轮已完成的部分工作（d1完成，d2部分完成），需要继续：

1. **展开 `07-PHASE7-cpp26-features.md` Task 7.5.1 和 7.5.2** — 将当前 ~40 行概括描述扩展为详细开发要点（含子任务、代码片段、Clang 参照）
2. **补充 `07-PHASE7-detailed-interface-plan.md` Task 7.5.1 接口定义** — 当前只有 checklist 条目，缺少实际接口代码
3. **更新 `07-PHASE7-feature-verification-report.md`** — 同步最新状态
4. **同步 `CPP23-CPP26-FEATURES.md`** — 解决 3 个状态冲突

## 核心特性

- Task 7.5.1 转义序列扩展：分隔转义 `\x{...}` (P2290R3) 和命名转义 `\N{...}` (P2071R2)
- Task 7.5.2 八个 C++26 子特性：for-using、扩展字符集、[[indeterminate]]、平凡可重定位、概念模板参数、可变参数友元、constexpr 异常、可观察检查点
- 所有特性需要参照 Clang 对应模块实现方案做决策

## 技术栈

- 文档类型：Markdown 开发计划文档
- 目标项目：BlockType C++26 编译器（LLVM 后端）
- 参考架构：Clang 编译器对应模块

## 实现方案

### 文档扩展策略

对每个待扩展的 Task，按照已完成的 Task 7.2.2 和 7.4.3 的格式为标准：

- 将概括性要点拆分为 `E7.x.x.x` 子任务
- 每个子任务包含：修改文件路径、代码片段（接口声明）、Clang 参照链接
- 添加完整的接口预置清单
- 添加验收标准 checklist

### 修改文件清单

```
project-root/
├── docs/plan/
│   ├── 07-PHASE7-cpp26-features.md          # [MODIFY] 展开 Task 7.5.1/7.5.2
│   ├── 07-PHASE7-detailed-interface-plan.md  # [MODIFY] 补充 Task 7.5.1 接口定义
│   ├── 07-PHASE7-feature-verification-report.md  # [MODIFY] 同步最新状态
│   └── CPP23-CPP26-FEATURES.md              # [MODIFY] 修复 3 个冲突状态
```

### 各文件具体修改

**`07-PHASE7-cpp26-features.md` Task 7.5.1（~20行→~80行）**

展开为 3 个子任务：

- E7.5.1.1: `\x{HHHH...}` 分隔转义 — Lexer.cpp 的 `processEscapeSequence()` 修改
- E7.5.1.2: `\N{name}` 命名转义 — Lexer.h 新增方法 + Unicode 字符名数据库
- E7.5.1.3: Unicode Character Database 集成方案

**`07-PHASE7-cpp26-features.md` Task 7.5.2（~20行→~200行）**

展开为 8 个子任务，每个包含：Clang 参照文件路径、修改文件、关键代码变更、验收标准。

**`07-PHASE7-detailed-interface-plan.md` Task 7.5.1（新增~60行）**

在 Stage 7.5 部分添加 Task 7.5.1 的详细接口：

- Lexer.h 新增方法声明：`processDelimitedHexEscape()`、`processNamedEscape()`
- TokenKinds.def 无需新增（转义序列不产生新 token）
- 诊断 ID：无效分隔转义、无效命名转义、未知字符名
- Unicode 字符名查找接口