# BlockType 编译器开发总规划

> **⚠️ 注意：** 本文档为初始规划文档（v1.0，2026-04-14），**不含 AI 功能模块**。
> 
> **最新状态请参考：**
> - [`docs/dev status/`](../dev%20status/) - 各 Phase 实际完成状态
> - [`docs/AI_API.md`](../AI_API.md) - AI 功能 API 文档
> - [`docs/AI_USAGE.md`](../AI_USAGE.md) - AI 功能使用指南
> - Git 提交历史 - 详细实现记录
> 
> **项目代号：** BlockType（曾用名：zetacc, Nova）
> **目标：** 从零开发 C++26 标准编译器前端，后端使用 LLVM，支持 Linux x86_64 和 macOS ARM64
> **代码生成：** GLM AI 大模型辅助开发
> **文档版本：** v1.0 | 2026-04-14（最后更新：2026-04-17）

---

## 一、项目总览

| 指标 | 目标 |
|------|------|
| 语言标准 | C++26（含 C++26 反射、Contracts、#embed 等新特性） |
| 目标平台 | Linux x86_64、macOS ARM64 |
| 后端 | LLVM 17+ |
| 前端语言 | C++20 |
| 开发周期 | 约 44-56 周（10-13 个月） |

---

## 二、Phase 总览

| Phase | 名称 | 核心交付物 | 预计时长 |
|-------|------|-----------|----------|
| Phase 0 | 基础设施 | CMake 构建系统、项目骨架、CI/CD | 2 周 |
| Phase 1 | 词法分析器 + 预处理器 | Lexer、Token 体系、宏展开、#embed | 6 周 |
| Phase 2 | 语法分析（表达式/语句） | Parser 表达式解析、语句解析 | 6 周 |
| Phase 3 | 语法分析（声明/类） | 声明解析、类定义、模板声明 | 8 周 |
| Phase 4 | 语义分析基础 | 类型检查、符号表、重载解析 | 6 周 |
| Phase 5 | 模板与泛型 | 模板实例化、Concept、SFINAE | 8 周 |
| Phase 6 | LLVM IR 生成 | AST → LLVM IR、调试信息 | 6 周 |
| Phase 7 | C++26 特性 | 反射、Contracts、协程扩展 | 6 周 |
| Phase 8 | 目标平台与优化 | 平台适配、优化 Pass | 4 周 |
| Phase 9 | 集成与生产化 | 完整测试、文档、发布 | 4 周 |

---

## 三、目录结构

```
blocktype/
├── CMakeLists.txt                 # 顶层 CMake
├── src/
│   ├── Basic/                     # 基础设施
│   ├── Lex/                       # 词法分析
│   ├── Parse/                     # 语法分析
│   ├── Sema/                      # 语义分析
│   ├── AST/                       # AST 节点
│   ├── IRGen/                     # IR 生成
│   └── Driver/                    # 编译器驱动
├── include/
│   └── blocktype/
│       ├── Basic/
│       ├── Lex/
│       ├── Parse/
│       └── ...
├── tests/
│   ├── unit/
│   └── regression/
└── docs/
    └── cpp26-compiler-plan/
        ├── 00-MASTER-PLAN.md
        ├── 01-PHASE0-foundation.md
        ├── 02-PHASE1-lexer-preprocessor.md
        └── ...
```

---

## 四、编译流水线

```
Source File (.cpp/.h)
    │
    ▼
┌─────────────────┐
│  SourceManager  │  ← 文件读取、缓冲区管理
└───────┬─────────┘
        ▼
┌─────────────────┐
│    Lexer        │  ← 词法分析 → Token 流
└───────┬─────────┘
        ▼
┌─────────────────┐
│  Preprocessor   │  ← 宏展开、#include
└───────┬─────────┘
        ▼
┌─────────────────┐
│    Parser       │  ← 语法分析 → AST
└───────┬─────────┘
        ▼
┌─────────────────┐
│     Sema        │  ← 语义分析、类型检查
└───────┬─────────┘
        ▼
┌─────────────────┐
│    IRGen        │  ← LLVM IR 生成
└───────┬─────────┘
        ▼
┌─────────────────┐
│  LLVM Backend   │  ← 优化、目标代码生成
└─────────────────┘
```

---

## 五、Phase 依赖关系

```
Phase 0 ──► Phase 1 ──► Phase 2 ──► Phase 3 ──► Phase 4 ──► Phase 5 ──► Phase 6 ──► Phase 7 ──► Phase 8 ──► Phase 9
   │            │           │           │           │           │           │           │           │           │
   │            │           │           │           │           │           │           │           │           │
   ▼            ▼           ▼           ▼           ▼           ▼           ▼           ▼           ▼           ▼
基础骨架    Lexer+PP    表达式      声明/类    类型检查    模板      IR 生成    C++26     平台适配    发布
```

---

## 六、文档索引

| 文档 | 内容 |
|------|------|
| `01-PHASE0-foundation.md` | Phase 0 详细开发计划 |
| `02-PHASE1-lexer-preprocessor.md` | Phase 1 详细开发计划 |
| `03-PHASE2-parser-expression.md` | Phase 2 详细开发计划（待创建） |
| `...` | ... |

---

*本文档为 BlockType 编译器开发的总体指导文档，各 Phase 的详细开发计划见对应文档。*
