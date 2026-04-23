# W6-P2-2: addCandidates 缺少模板参数推导步骤 — 完整解决方案

> **文档版本**: v1.0 | **日期**: 2026-04-23
> **作者**: planner | **问题来源**: Wave6 Review
> **优先级**: P2 | **状态**: 规划完成，待实施

---

## 一、问题定义

### 1.1 问题描述

`OverloadCandidateSet::addCandidates(const LookupResult &R)` 在遇到 `FunctionTemplateDecl` 时，直接将模板的泛型函数声明（`TemplatedDecl`）添加为候选，**没有执行模板参数推导**。这导致候选函数的参数类型仍包含 `TemplateTypeParmType`（依赖类型），在 `resolve()` 的 `checkViability()` 中会被拒绝（类型不匹配），使得模板函数无法参与重载决议。

### 1.2 当前数据流（有问题）

```
SemaExpr::ActOnCallExpr
  ├─ 路径A: 直接识别 FunctionTemplateDecl
  │   └─ DeduceAndInstantiateFunctionTemplate(FTD, Args)  ← 正确但绕过了重载决议
  │
  └─ 路径B: 通过 ResolveOverload (重载决议)
      ├─ addCandidates(LR)  ← 遇到 FTD 只添加 TemplatedDecl，不推导
      └─ resolve(Args)
          └─ checkViability(Args)  ← 拒绝含 TemplateTypeParmType 的候选

问题：路径A 只处理单一模板调用，路径B 无法处理模板+非模板混合重载
```

### 1.3 正确的 C++ 流程（[temp.deduct.call]）

```
1. 从调用参数推导模板参数: Deduce(ParamType, ArgType) → DeducedArgs
2. 替换到签名生成具体 FunctionDecl: Substitute(TemplatedDecl, DeducedArgs) → Specialization
3. 添加具体特化作为候选: addCandidate(Specialization)
4. 对所有候选执行 viability 检查和排名
```

### 1.4 影响范围

| 场景 | 当前行为 | 正确行为 |
|------|---------|---------|
| 单一模板函数调用 | 路径A 正确处理 | 不变 |
| 模板+非模板重载 `f(int)` vs `f(T)` | 模板候选被拒绝，只选非模板 | 两者均参与决议 |
| 多个模板重载 `f(T*)` vs `f(T&)` | 均被拒绝 → NoViable | 推导后比较特化程度 |
| 显式模板参数 `f<int>(x)` | 路径A 处理 | 不变 |

---

## 二、前置架构调整

### 2.1 核心矛盾：`addCandidates` 没有 `Args`

当前 `addCandidates(const LookupResult &R)` 只接收查找结果，没有调用参数。模板参数推导需要调用参数类型，但 `Args` 只在 `resolve()` 阶段才可用。

### 2.2 方案选择

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| **A: 改 addCandidates 签名** | `addCandidates(R, Args, SemaRef)` | 推导和添加合一 | 改变 API，需更新所有调用点 |
| **B: 在 resolve 中延迟推导** | `resolve()` 遍历候选时对模板候选执行推导 | 不改 addCandidates API | resolve 职责膨胀 |
| **C: 新增 deduceTemplateCandidates 步骤** | 在 `addCandidates` 和 `resolve` 之间插入独立步骤 | 职责分离清晰，最小改动 | 需要暴露 Args 给中间步骤 |

**推荐方案 C**：在 `Sema::ResolveOverload()` 中，在 `addCandidates()` 之后、`resolve()` 之前，插入一个 `deduceTemplateCandidates(Args)` 步骤。理由：

1. **最小侵入**：不改变 `addCandidates` 和 `resolve` 的现有签名
2. **职责分离**：推导是 Sema 层的语义操作，不应内嵌到 OverloadCandidateSet 的数据结构方法中
3. **SFINAE 正确性**：推导失败应从候选集中移除候选（SFINAE），这需要 Sema 的诊断引擎支持
4. **与 Clang 架构一致**：Clang 在 `Sema::AddTemplateOverloadCandidate` 中执行推导，而非在候选集内部

### 2.3 架构调整清单

#### 调整 1: OverloadCandidate 增加推导状态字段

```cpp
// include/blocktype/Sema/Overload.h — OverloadCandidate 类
class OverloadCandidate {
  // ... 现有字段 ...

  /// 推导结果：如果此候选来自模板且推导已执行，记录结果
  TemplateDeductionResult DeductionResult = TemplateDeductionResult::Success;

  /// 推导信息：记录具体推导出的模板参数
  std::unique_ptr<TemplateDeductionInfo> DeductionInfo;

public:
  /// 标记此候选的推导结果
  void setDeductionResult(TemplateDeductionResult R,
                           std::unique_ptr<TemplateDeductionInfo> Info) {
    DeductionResult = R;
    DeductionInfo = std::move(Info);
  }

  TemplateDeductionResult getDeductionResult() const { return DeductionResult; }
  bool isDeductionFailure() const {
    return DeductionResult != TemplateDeductionResult::Success;
  }
};
```

#### 调整 2: OverloadCandidateSet 增加 deduceTemplateCandidates 方法

```cpp
// include/blocktype/Sema/Overload.h — OverloadCandidateSet 类
class OverloadCandidateSet {
  // ... 现有字段 ...

  /// Sema 引用（用于执行推导和实例化）
  class Sema *SemaRef = nullptr;

public:
  /// 设置 Sema 引用（在 resolve 前调用）
  void setSema(class Sema &S) { SemaRef = &S; }

  /// 对所有模板候选执行模板参数推导
  /// 推导成功的候选：替换为具体特化 FunctionDecl
  /// 推导失败的候选：标记为不可行（SFINAE）
  void deduceTemplateCandidates(llvm::ArrayRef<Expr *> Args);
};
```

#### 调整 3: Sema::ResolveOverload 调整调用流程

```cpp
// src/Sema/Sema.cpp — 修改后的 ResolveOverload
FunctionDecl *Sema::ResolveOverload(StringRef Name,
                                     ArrayRef<Expr *> Args,
                                     const LookupResult &Candidates) {
  OverloadCandidateSet OCS(CallLoc);
  OCS.setSema(*this);           // [新增] 设置 Sema 引用
  OCS.setConstraintChecker(ConstraintChecker.get());

  OCS.addCandidates(Candidates);
  OCS.deduceTemplateCandidates(Args);  // [新增] 推导模板参数

  auto [Result, Best] = OCS.resolve(Args);
  // ... 后续不变 ...
}
```

---

## 三、具体修复步骤

### 步骤 1: 修改 `include/blocktype/Sema/Overload.h`

**改动**:
1. `OverloadCandidate` 增加 `DeductionResult` 和 `DeductionInfo` 字段及访问器
2. `OverloadCandidateSet` 增加 `SemaRef` 字段、`setSema()` 和 `deduceTemplateCandidates()` 声明
3. 添加必要的 `#include` 前置声明

**代码变更**:
```cpp
// 新增 include
#include "blocktype/Sema/TemplateDeduction.h"  // TemplateDeductionResult, TemplateDeductionInfo

// OverloadCandidate 新增:
private:
  TemplateDeductionResult DeductionResult = TemplateDeductionResult::Success;
  std::unique_ptr<TemplateDeductionInfo> DeductionInfo;
public:
  void setDeductionResult(TemplateDeductionResult R,
                           std::unique_ptr<TemplateDeductionInfo> Info);
  TemplateDeductionResult getDeductionResult() const;
  bool isDeductionFailure() const;

// OverloadCandidateSet 新增:
private:
  class Sema *SemaRef = nullptr;
public:
  void setSema(class Sema &S);
  void deduceTemplateCandidates(llvm::ArrayRef<Expr *> Args);
```

### 步骤 2: 实现 `deduceTemplateCandidates` — `src/Sema/Overload.cpp`

**核心逻辑**:

```cpp
void OverloadCandidateSet::deduceTemplateCandidates(
    llvm::ArrayRef<Expr *> Args) {
  if (!SemaRef) return;

  for (auto &C : Candidates) {
    // 只处理来自函数模板的候选
    FunctionTemplateDecl *FTD = C.getTemplate();
    if (!FTD) continue;

    FunctionDecl *TemplatedFD = C.getFunction();
    if (!TemplatedFD) continue;

    // 检查参数类型是否包含未解析的模板参数
    // 如果所有参数类型都是具体类型，推导已隐式完成，跳过
    bool HasDependentParam = false;
    for (unsigned I = 0; I < TemplatedFD->getNumParams(); ++I) {
      QualType ParamTy = TemplatedFD->getParamDecl(I)->getType();
      if (ParamTy.isNull()) continue;
      const Type *Ty = ParamTy.getTypePtr();
      if (llvm::isa<TemplateTypeParmType>(Ty) ||
          Ty->getTypeClass() == TypeClass::Dependent) {
        HasDependentParam = true;
        break;
      }
    }
    if (!HasDependentParam) continue;

    // 执行模板参数推导
    TemplateDeductionInfo Info;
    TemplateDeductionResult DedResult =
        SemaRef->getDeduction()->DeduceFunctionTemplateArguments(
            FTD, Args, Info);

    if (DedResult != TemplateDeductionResult::Success) {
      // SFINAE: 推导失败，标记候选为不可行
      C.setViable(false);
      C.setDeductionResult(DedResult,
          std::make_unique<TemplateDeductionInfo>(std::move(Info)));
      switch (DedResult) {
      case TemplateDeductionResult::TooFewArguments:
        C.setFailureReason("template deduction: too few arguments");
        break;
      case TemplateDeductionResult::TooManyArguments:
        C.setFailureReason("template deduction: too many arguments");
        break;
      case TemplateDeductionResult::Inconsistent:
        C.setFailureReason("template deduction: inconsistent");
        break;
      default:
        C.setFailureReason("template deduction failed");
        break;
      }
      continue;
    }

    // 推导成功：收集推导出的模板参数
    auto *Params = FTD->getTemplateParameterList();
    unsigned NumParams = Params ? Params->size() : 0;
    llvm::SmallVector<TemplateArgument, 4> DeducedArgs =
        Info.getDeducedArgs(NumParams);

    // 检查 requires-clause 约束（如果有）
    if (FTD->hasRequiresClause()) {
      Expr *RequiresClause = FTD->getRequiresClause();
      bool Satisfied =
          SemaRef->getConstraintChecker()->CheckConstraintSatisfaction(
              RequiresClause, DeducedArgs);
      if (!Satisfied) {
        C.setViable(false);
        C.setDeductionResult(
            TemplateDeductionResult::SubstitutionFailure, nullptr);
        C.setFailureReason("constraint not satisfied");
        continue;
      }
    }

    // 实例化：用推导出的参数替换模板签名
    FunctionDecl *Specialization =
        SemaRef->InstantiateFunctionTemplate(FTD, DeducedArgs);

    if (!Specialization) {
      C.setViable(false);
      C.setDeductionResult(
          TemplateDeductionResult::SubstitutionFailure, nullptr);
      C.setFailureReason("template instantiation failed");
      continue;
    }

    // 替换候选的 FunctionDecl 为具体特化
    // 注意：OverloadCandidate::Function 是私有字段，需要添加 setter
    C.setFunction(Specialization);
    C.setDeductionResult(TemplateDeductionResult::Success,
        std::make_unique<TemplateDeductionInfo>(std::move(Info)));
  }
}
```

### 步骤 3: 修改 `OverloadCandidate` — 添加 `setFunction`

```cpp
// include/blocktype/Sema/Overload.h
class OverloadCandidate {
  // ...
public:
  void setFunction(FunctionDecl *F) { Function = F; }
};
```

### 步骤 4: 修改 `Sema::ResolveOverload` — `src/Sema/Sema.cpp`

```cpp
FunctionDecl *Sema::ResolveOverload(llvm::StringRef Name,
                                     llvm::ArrayRef<Expr *> Args,
                                     const LookupResult &Candidates) {
  SourceLocation CallLoc;
  OverloadCandidateSet OCS(CallLoc);
  OCS.setSema(*this);                              // [新增]
  OCS.setConstraintChecker(ConstraintChecker.get()); // 已有

  OCS.addCandidates(Candidates);
  OCS.deduceTemplateCandidates(Args);              // [新增]

  if (OCS.empty()) {
    Diags.report(SourceLocation(), DiagID::err_ovl_no_viable_function, Name);
    return nullptr;
  }

  auto [Result, Best] = OCS.resolve(Args);
  // ... 后续诊断逻辑不变 ...
}
```

### 步骤 5: 添加 Sema 访问器

```cpp
// include/blocktype/Sema/Sema.h — 新增公共访问器
public:
  TemplateDeduction *getDeduction() const { return Deduction.get(); }
  ConstraintSatisfaction *getConstraintChecker() const { return ConstraintChecker.get(); }
  TemplateInstantiator *getInstantiator() const { return Instantiator.get(); }
```

### 步骤 6: 修改 `resolve()` — 跳过已标记不可行的候选

```cpp
// src/Sema/Overload.cpp — resolve() 中
std::pair<OverloadResult, FunctionDecl *>
OverloadCandidateSet::resolve(llvm::ArrayRef<Expr *> Args) {
  // 1. Check viability of all candidates
  for (auto &C : Candidates) {
    // 跳过已被 deduceTemplateCandidates 标记为不可行的候选
    if (C.isDeductionFailure()) continue;
    C.checkViability(Args);
  }
  // ... 后续不变 ...
}
```

### 步骤 7: 更新 `addCandidates` 中的 TODO 注释

移除 TODO(W6-P2-2) 注释，替换为说明推导已在 `deduceTemplateCandidates` 中完成：

```cpp
} else if (auto *FTD = llvm::dyn_cast<FunctionTemplateDecl>(D)) {
  // Template candidates are added with their templated declaration.
  // Template argument deduction is performed later in
  // deduceTemplateCandidates() when call arguments are available.
  if (auto *TemplatedFD = llvm::dyn_cast_or_null<FunctionDecl>(
          FTD->getTemplatedDecl())) {
    addTemplateCandidate(TemplatedFD, FTD);
  }
}
```

---

## 四、需要修改的文件和函数列表

| 文件 | 修改类型 | 函数/类 | 说明 |
|------|---------|---------|------|
| `include/blocktype/Sema/Overload.h` | 修改 | `OverloadCandidate` | 添加 `DeductionResult`、`DeductionInfo`、`setFunction()`、推导访问器 |
| `include/blocktype/Sema/Overload.h` | 修改 | `OverloadCandidateSet` | 添加 `SemaRef`、`setSema()`、`deduceTemplateCandidates()` 声明 |
| `include/blocktype/Sema/Sema.h` | 修改 | `Sema` | 添加 `getDeduction()`、`getConstraintChecker()`、`getInstantiator()` 访问器 |
| `src/Sema/Overload.cpp` | 修改 | `OverloadCandidateSet::addCandidates` | 移除 TODO，更新注释 |
| `src/Sema/Overload.cpp` | 新增 | `OverloadCandidateSet::deduceTemplateCandidates` | 核心推导逻辑实现 |
| `src/Sema/Overload.cpp` | 修改 | `OverloadCandidateSet::resolve` | 跳过推导失败的候选 |
| `src/Sema/Sema.cpp` | 修改 | `Sema::ResolveOverload` | 添加 `setSema()` 和 `deduceTemplateCandidates()` 调用 |

**总计**: 3 个头文件修改 + 2 个源文件修改，约 120 行新增代码

---

## 五、风险评估

### 5.1 风险矩阵

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 推导+实例化创建的 FunctionDecl 内存泄漏 | 中 | 低 | 使用 ASTContext::create 管理，BumpPtrAllocator 自动回收 |
| 推导成功但实例化失败（复杂模板） | 中 | 中 | 实例化失败时标记候选不可行，不阻断编译 |
| 现有路径A（DeduceAndInstantiate）重复推导 | 低 | 低 | 路径A 在路径B 之前执行，不会重复；后续可统一 |
| findSpecialization 返回 nullptr（未实现） | 高 | 中 | InstantiateFunctionTemplate 内部会创建新特化，不依赖缓存查找 |
| SFINAE 上下文未正确管理 | 中 | 高 | 在 deduceTemplateCandidates 中进入 SFINAE 上下文，推导失败不报错 |
| 循环依赖：Overload.h → Sema.h | 低 | 编译 | 使用前向声明 `class Sema`，实现放在 .cpp 中 |

### 5.2 关键风险详解

**R1: SFINAE 上下文管理**

模板参数推导失败在 C++ 中是 SFINAE（Substitution Failure Is Not An Error），即推导失败只是将候选从重载集中移除，不应报错。当前 `DeduceAndInstantiateFunctionTemplate` 已有 SFINAE Guard 实现，`deduceTemplateCandidates` 需要复用相同模式：

```cpp
// 在 deduceTemplateCandidates 内部
unsigned SavedErrors = SemaRef->getDiagnostics().getNumErrors();
{
  SFINAEGuard Guard(SemaRef->getInstantiator()->getSFINAEContext(),
                    SavedErrors, &SemaRef->getDiagnostics());
  DedResult = SemaRef->getDeduction()->DeduceFunctionTemplateArguments(
      FTD, Args, Info);
}
// Guard 析构后恢复诊断
```

**R2: 循环头文件依赖**

`Overload.h` 不能 `#include "Sema.h"`（循环依赖）。解决方案：
- `Overload.h` 中只前向声明 `class Sema`
- `Overload.cpp` 中 `#include "blocktype/Sema/Sema.h"` 获取完整定义
- `deduceTemplateCandidates` 的实现在 `Overload.cpp` 中，可访问 Sema 的完整接口

**R3: findSpecialization 未实现**

当前 `FunctionTemplateDecl::findSpecialization()` 返回 nullptr（TODO 未实现）。这意味着每次推导都会创建新的特化 FunctionDecl，不会复用已有特化。影响：
- 功能正确性：不受影响（新创建的特化功能等价）
- 性能：轻微退化（重复创建相同特化），但重载决议只在调用点执行一次，影响可忽略
- 后续改进：实现 `findSpecialization` 后自动获得缓存复用

---

## 六、测试策略

### 6.1 单元测试（新增）

| 测试用例 | 文件 | 说明 |
|---------|------|------|
| `DeduceTemplateCandidate_SingleTemplate` | `OverloadResolutionTest.cpp` | `f(T)` 调用 `f(42)` → 推导 T=int，候选可行 |
| `DeduceTemplateCandidate_TemplatePlusNonTemplate` | `OverloadResolutionTest.cpp` | `f(int)` + `f(T)` 调用 `f(42)` → 两个候选均可行，非模板优先 |
| `DeduceTemplateCandidate_DeductionFailure_SFINAE` | `OverloadResolutionTest.cpp` | `f(T*)` 调用 `f(42)` → 推导失败，候选移除，不报错 |
| `DeduceTemplateCandidate_TwoTemplates` | `OverloadResolutionTest.cpp` | `f(T*)` + `f(T&)` 调用 `f(&x)` → 两个推导成功，比较转换排名 |
| `DeduceTemplateCandidate_ExplicitArgs` | `OverloadResolutionTest.cpp` | 显式模板参数 `f<int>(x)` → 跳过推导 |
| `DeduceTemplateCandidate_ConstraintReject` | `OverloadResolutionTest.cpp` | `f(T)` requires `is_integral<T>` 调用 `f(3.14)` → 约束不满足，候选移除 |

### 6.2 集成测试（端到端）

| 测试场景 | 预期行为 |
|---------|---------|
| `template<typename T> void f(T); f(42);` | 推导 T=int，成功调用 |
| `void f(int); template<typename T> void f(T); f(42);` | 非模板优先 |
| `template<typename T> void f(T*); int x; f(&x);` | 推导 T=int，成功调用 |
| `template<typename T> void f(T*); f(42);` | 推导失败，NoViable（SFINAE 不报错） |
| `template<typename T> void f(T); template<> void f(int); f(42);` | 显式特化优先 |

### 6.3 回归测试

- 运行完整测试套件（766 测试），确保不引入新的失败
- 重点关注 `Sema/OverloadResolutionTest.cpp` 和 `Sema/TemplateDeductionTest.cpp`

### 6.4 测试优先级

1. **P0**: 现有 766 测试全部通过（回归）
2. **P1**: 新增 6 个单元测试通过
3. **P2**: 5 个端到端集成测试通过

---

## 七、工作量估算

| 步骤 | 工作量 | 说明 |
|------|--------|------|
| 步骤 1: 修改 Overload.h | 0.5 小时 | 添加字段和方法声明 |
| 步骤 2: 实现 deduceTemplateCandidates | 2 小时 | 核心逻辑，含 SFINAE 处理 |
| 步骤 3: 添加 setFunction | 0.25 小时 | 单行 setter |
| 步骤 4: 修改 ResolveOverload | 0.5 小时 | 两行新增 |
| 步骤 5: 添加 Sema 访问器 | 0.25 小时 | 三行 getter |
| 步骤 6: 修改 resolve | 0.5 小时 | 一行条件跳过 |
| 步骤 7: 更新注释 | 0.25 小时 | 文档更新 |
| 单元测试编写 | 2 小时 | 6 个测试用例 |
| 集成测试 | 1 小时 | 5 个端到端场景 |
| 调试和修复 | 1.5 小时 | 预留 |
| **总计** | **~8.5 小时** | **约 1.5 个工作日** |

---

## 八、后续改进（不在本次范围内）

| 改进 | 优先级 | 说明 |
|------|--------|------|
| 实现 `FunctionTemplateDecl::findSpecialization` | P2 | 缓存复用已有特化，避免重复创建 |
| 统一路径A和路径B | P2 | 移除 `ActOnCallExpr` 中的 `DeduceAndInstantiateFunctionTemplate` 直接调用，统一走重载决议 |
| 部分排序完善 | P2 | 多个模板候选推导成功后，使用 `isMoreSpecialized` 进行偏序选择 |
| 显式模板参数处理 | P1 | `f<int>(x)` 应跳过推导，直接用显式参数实例化 |
| 诊断信息增强 | P3 | 推导失败时输出详细的推导步骤和失败原因 |

---

## 九、与 Clang 架构的对比

| 方面 | Clang | BlockType（本方案后） |
|------|-------|---------------------|
| 模板候选添加 | `Sema::AddTemplateOverloadCandidate` 执行推导 | `deduceTemplateCandidates` 执行推导 |
| 推导时机 | 添加候选时（需 Args） | 添加候选后、resolve 前（需 Args） |
| SFINAE 处理 | `Sema::SFINAETrap` + 诊断抑制 | `SFINAEGuard` + 诊断抑制 |
| 特化缓存 | `FunctionTemplateDecl::findSpecialization` 完整实现 | TODO（当前返回 nullptr） |
| 候选替换 | 推导成功后替换为特化 | 推导成功后替换为特化（一致） |

本方案在语义上与 Clang 一致，仅在执行时机上略有差异（Clang 在添加时推导，我们在添加后统一推导），不影响正确性。

---

*本文档为 W6-P2-2 问题的完整解决方案规划。实施时应按步骤顺序执行，每步完成后运行测试验证。*
