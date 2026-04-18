---
name: Stage6-1-IRGen-Audit
overview: 全面核查 Stage 6.1 IRGen 基础设施代码，对照开发文档和 Clang 对应模块，生成核查报告并修复所有问题
todos:
  - id: fix-emit-tu
    content: 修复 EmitTranslationUnit 两遍发射逻辑断裂（P0）+ linkage 判断（P1）
    status: completed
  - id: fix-types-h
    content: 补充 CodeGenTypes.h 缺失接口声明 + 实现 this 指针/枚举底层类型/FunctionTypeCache
    status: completed
  - id: fix-constant-h
    content: 补充 CodeGenConstant.h 缺失接口声明 + 实现类型转换常量方法
    status: completed
  - id: fix-targetinfo
    content: 修复 TargetInfo LongDouble 平台适配 + 枚举底层类型支持
    status: completed
  - id: build-test
    content: 编译全项目并运行 568 个测试验证无回归
    status: completed
    dependencies:
      - fix-emit-tu
      - fix-types-h
      - fix-constant-h
      - fix-targetinfo
  - id: write-report
    content: 生成 Phase6-AUDIT-Stage6-1 全面核查报告并保存到 docs/dev status
    status: completed
    dependencies:
      - build-test
---

## 产品概述

对 BlockType 编译器项目 Stage 6.1 IRGen 基础设施代码进行全面核查，对照开发文档 docs/plan/06-PHASE6-irgen.md 的 Stage 6.1 章节（Task 6.1.1-6.1.4），逐项检查实现代码的完整性、接口一致性、逻辑正确性，并参照 Clang CodeGen 模块的对应特性给出核查报告。

## 核心功能

- 对比开发文档中声明的全部接口与实际头文件/实现，列出缺失项
- 检查 EmitTranslationUnit 两遍发射逻辑是否完整正确
- 检查类型映射覆盖率和映射规则正确性
- 检查常量生成覆盖率和类型安全
- 检查 TargetInfo 平台适配细节
- 检查组件间关联关系（CodeGenModule ↔ Types ↔ Constant ↔ TargetInfo）
- 参照 Clang CodeGenModule/CodeGenTypes/ConstantEmitter/TargetInfo 给出差距分析
- 按优先级（P0/P1/P2）分类所有发现的问题

## 修复策略

基于核查发现的 17 个问题（1 个 P0、8 个 P1、8 个 P2），按优先级分批修复。

### P0: EmitTranslationUnit 两遍发射逻辑断裂

**根因**: `EmitDeferred()` 中第二遍函数体生成代码是空循环（只跳过 declaration，没有代码调用 `EmitFunction`）。
**修复**: 需要保存 TU 指针或在第一遍中记录有 body 的 FunctionDecl，在 `EmitDeferred` 中对它们调用 `EmitFunction`。

### P1: 缺失接口补充

需要补充 CodeGenTypes.h 中 `ConvertTypeForValue`、`GetCXXRecordType`、`GetSize`/`GetAlign`，以及 CodeGenConstant.h 中 `EmitIntCast`/`EmitFloatToIntCast`/`EmitIntToFloatCast`/`GetNullPointer`。

### P1: Linkage/属性/this 指针

需要根据 Decl 属性（static、inline、const 等）选择正确的 linkage 和属性。`GetFunctionTypeForDecl` 需要为非 static 成员函数添加 this 指针参数。

## 实现方案

### 批次 1: P0 + 关键 P1（4 个文件）

- `src/CodeGen/CodeGenModule.cpp` — 修复两遍发射逻辑 + linkage 判断
- `include/blocktype/CodeGen/CodeGenTypes.h` — 补充缺失接口声明
- `src/CodeGen/CodeGenTypes.cpp` — 实现补充接口 + this 指针处理 + 枚举底层类型
- `include/blocktype/CodeGen/CodeGenConstant.h` — 补充缺失接口声明

### 批次 2: 剩余 P1 + P2 实现

- `src/CodeGen/CodeGenConstant.cpp` — 实现类型转换常量方法
- `src/CodeGen/TargetInfo.cpp` — LongDouble 平台适配
- `src/CodeGen/CodeGenTypes.cpp` — 补充 FunctionTypeCache

### 目录结构

```
project-root/
├── include/blocktype/CodeGen/
│   ├── CodeGenModule.h          # [不变]
│   ├── CodeGenTypes.h           # [修改] 补充 4 个缺失接口声明
│   ├── CodeGenConstant.h        # [修改] 补充 4 个缺失接口声明
│   ├── CodeGenFunction.h        # [不变]
│   ├── CGCXX.h                  # [不变]
│   └── TargetInfo.h             # [不变]
├── src/CodeGen/
│   ├── CodeGenModule.cpp        # [修改] P0: 两遍发射逻辑修复 + P1: linkage
│   ├── CodeGenTypes.cpp         # [修改] P1: this指针 + 枚举 + 缺失接口实现 + FunctionTypeCache
│   ├── CodeGenConstant.cpp      # [修改] P1: 类型转换常量实现
│   ├── CGCXX.cpp                # [不变]
│   └── TargetInfo.cpp           # [修改] P2: LongDouble 适配
└── docs/dev status/
    └── Phase6-AUDIT-Stage6-1 全面核查报告.md  # [新建] 核查报告
```

## 关键代码结构

### EmitTranslationUnit 修复方案

```cpp
void CodeGenModule::EmitTranslationUnit(TranslationUnitDecl *TU) {
  if (!TU) return;
  // 第一遍：声明
  SmallVector<FunctionDecl*, 32> FunctionsWithBodies;
  for (Decl *D : TU->decls()) {
    if (auto *FD = dyn_cast<FunctionDecl>(D)) {
      GetOrCreateFunctionDecl(FD);
      if (FD->getBody()) FunctionsWithBodies.push_back(FD);
    } else if (...) { ... }
  }
  // 延迟：全局变量
  EmitDeferred();
  // 第二遍：函数体
  for (FunctionDecl *FD : FunctionsWithBodies) {
    EmitFunction(FD);
  }
  // 全局构造/析构
  EmitGlobalCtorDtors();
}
```

### GetFunctionTypeForDecl this 指针处理

```cpp
llvm::FunctionType *CodeGenTypes::GetFunctionTypeForDecl(FunctionDecl *FD) {
  QualType FT = FD->getType();
  auto *FnTy = dyn_cast<FunctionType>(FT.getTypePtr());
  if (!FnTy) return /* fallback */;
  
  // 成员函数：添加 this 指针作为第一个参数
  if (auto *MD = dyn_cast<CXXMethodDecl>(FD)) {
    if (!MD->isStatic()) {
      SmallVector<llvm::Type*, 8> ParamTys;
      // this 指针类型
      llvm::StructType *ClassTy = GetRecordType(MD->getParent());
      ParamTys.push_back(llvm::PointerType::get(ClassTy, 0));
      // 原始参数
      for (const Type *PT : FnTy->getParamTypes())
        ParamTys.push_back(ConvertType(QualType(PT, Qualifier::None)));
      llvm::Type *RetTy = ConvertType(QualType(FnTy->getReturnType(), Qualifier::None));
      return llvm::FunctionType::get(RetTy, ParamTys, FnTy->isVariadic());
    }
  }
  return GetFunctionType(FnTy);
}
```