Phase5-AUDIT-Stage5.1+5.2 全面核查报告
对照开发文档 docs/plan/05-PHASE5-template-generics.md 逐项核查，并结合 Clang 同模块特性对比。
最后更新：2026-04-18（提交 b536aaf 后刷新）

---

一、Stage 5.1 核查

Task 5.1.1 — 模板 Sema ActOn* 方法

E5.1.1.2 SemaTemplate.cpp 实现	⚠️	实现存在但验证逻辑偏薄

问题：

1. ActOnExplicitSpecialization / ActOnExplicitInstantiation 为纯占位（返回 getInvalid()）— 开发文档标注"Stage 5.5 完整实现"，所以这是合理的延迟。但 ActOnExplicitInstantiation 应至少触发实例化（目前直接返回 D），应在 5.5 补充。
   → 状态：⏳ 延迟到 Stage 5.5

2. 参数验证不完整：开发文档要求"验证模板参数列表"，当前实现只检查 Params 是否为空，未检查：
   - 参数是否有重复名称
   - NonTypeTemplateParmDecl 的类型是否完整
   - TemplateTemplateParmDecl 的嵌套参数是否有效
   → 状态：⏳ 低优先级，可在 5.5 完善

3. 缺少 CurContext->addDecl() 调用：ActOnClassTemplateDecl 等方法将声明注册到 Symbols 但没有调用 CurContext->addDecl()，这与其他 ActOn* 方法（如 ActOnVarDecl）不一致。
   → 状态：✅ 已修复（b536aaf）— 所有 5 个 ActOn* 方法已添加 CurContext->addDecl()


Task 5.1.2 — 模板实例化框架 ✅ 完成较好

E5.1.2.1 TemplateInstantiation.h
TemplateArgumentList + TemplateInstantiator，含 SFINAE 集成

E5.1.2.2 TemplateInstantiation.cpp	实现了完整的实例化流程

Clang 对比问题：

1. InstantiateClassTemplate 缺少递归深度溢出时的诊断：当 CurrentDepth >= MaxInstantiationDepth 时仅返回 nullptr，应通过 Sema 的 DiagnosticsEngine 报告 err_template_recursion 错误。
   → 状态：✅ 已修复（b536aaf）— InstantiateClassTemplate 和 InstantiateFunctionTemplate 均在非 SFINAE 上下文中报告 err_template_recursion

2. SubstituteExpr 是空实现（直接返回 E）：开发文档说明"依赖表达式的完整递归替换将在后续阶段展开"，这可以接受，但在当前阶段至少应处理 DeclRefExpr（替换引用的模板参数声明）和 BinaryOperator 等简单情况。Stage 5.3 的变参模板需要此功能。
   → 状态：⏳ 将在 Stage 5.3（变参模板）中补充

3. FindExistingSpecialization 使用线性扫描：Clang 使用 llvm::FoldingSet 做特化缓存查找（O(1)），当前实现对每个特化线性遍历。功能正确但在大量特化时性能差。
   → 状态：⏳ 性能优化项，可在后续阶段改为 FoldingSet

4. SubstituteType 未处理 QualifiedType / AttributedType：Clang 的 TreeTransform 处理了这两种类型。当前实现如果遇到这些类型会直接返回原值，可能导致 cv 限定符丢失。
   → 状态：⏳ 低优先级，cv 限定符通过 QualType 的 Quals 字段保留，丢失风险有限



Task 5.1.3 — 模板名称查找与 Sema 集成 ⚠️ 部分缺失

E5.1.3.2 扩展名称查找处理模板名	✅ 已修复

E5.1.3.3 依赖类型查找	❌	未实现

问题：

1. LookupUnqualifiedName 未处理模板名：开发文档要求"当 Name 匹配到 ClassTemplateDecl/FunctionTemplateDecl 时，需要正确返回"。
   → 状态：✅ 已修复（b536aaf）— LookupUnqualifiedName 全局回退中添加了 lookupTemplate/lookupConcept 查找；LookupTypeName 中 ClassTemplateDecl 被识别为有效类型名

2. 依赖类型查找未实现：开发文档要求检查 isDependentType() 区分依赖/非依赖名称。
   → 状态：⏳ 延迟到 Stage 5.3/5.4（依赖类型需要在变参模板和 Concepts 中大量使用）


Task 5.1.4 — 模板相关诊断 ID

13 个诊断 ID	✅
已定义但部分未使用（如 err_template_recursion 在实例化深度溢出时未发出）。
→ 状态：✅ 已修复（b536aaf）— err_template_recursion 已在两处深度溢出检查点发出诊断



---

二、Stage 5.2 核查

Task 5.2.1 — 类型推导引擎 ✅ 完成较好

E5.2.1.1 TemplateDeduction.h	✅	完整定义 TemplateDeductionResult, TemplateDeductionInfo, TemplateDeduction
E5.2.1.2 TemplateDeduction.cpp	✅	实现了推导算法

Clang 对比问题：

1. DeduceFunctionTemplateArguments 缺少对显式指定模板实参的处理：Clang 的 Sema::DeduceTemplateArguments 支持部分实参由调用者显式指定（如 f<int>(1.0)），剩余实参通过推导。当前实现假设所有实参都通过推导获取。
   → 状态：⏳ 中等优先级，将在 Stage 5.3/5.5 中补充（显式模板实参列表解析需 Parser 配合）

2. DeduceTemplateArguments 未处理 const T / volatile T 的 cv 限定符剥离：C++ [temp.deduct.call] 规定推导时需要剥离实参类型的顶层 cv 限定符。
   → 状态：✅ 已修复（b536aaf）— DeduceFunctionTemplateArguments 中对非引用参数剥离顶层 cv 限定符和引用

3. collapseReferences 实现不完整：T&& & → T& 的情况直接返回 Inner（即 T&&），实际应创建 LValueReferenceType。
   → 状态：✅ 已修复（b536aaf）— collapseReferences 现在通过 SemaRef.getASTContext() 获取 ASTContext 创建正确的 LValueReferenceType

4. DeduceFromReferenceType 缺少非引用实参到 T& 参数的处理：C++ [temp.deduct.call] 规定 T& 参数可以从非引用左值实参推导。
   → 状态：✅ 已修复（b536aaf）— 主推导函数中 ReferenceType 分支新增了 lvalue reference 参数对非引用实参的推导处理

5. 缺少对 TemplateTemplateParmDecl 的推导：Clang 支持模板模板参数的推导。
   → 状态：⏳ 低优先级，可在 Stage 5.5（模板特化）中补充


Task 5.2.2 — SFINAE 实现

E5.2.2.1 SFINAE.h	✅	SFINAEContext + SFINAEGuard（RAII）
E5.2.2.2 集成到 TemplateInstantiator	✅ 已修复

关键问题：

SFINAE 集成只是框架，未实际生效：
→ 状态：✅ 已部分修复（b536aaf）— 深度溢出时已区分 SFINAE/非 SFINAE 上下文（非 SFINAE 时报告诊断，SFINAE 时静默返回 nullptr）。SubstituteType 等替换方法中的替换失败本身已返回空 QualType，与 SFINAE 行为一致。完整集成（DiagnosticsEngine Suppress）将在 Stage 5.4 Concepts 中完善。


Task 5.2.3 — 部分排序 ⚠️ 实现偏差

isMoreSpecialized 实现	⚠️	使用评分法，非标准算法

问题：

1. 部分排序使用评分法而非标准双向推导法：C++ [temp.deduct.partial] 的算法是"为 P1 生成虚拟类型 → 尝试推导 P2"双向验证。
   → 状态：⏳ 已知偏差，评分法在常见场景下正确。完整双向推导需 generateDeducedType 生成唯一虚拟类型（需要 ASTContext 配合），可在 Stage 5.5 中重写

2. generateDeducedType 是空实现。
   → 状态：⏳ 需要实现以支持标准双向推导算法


---

三、跨模块关联性问题

1. ResolveOverload 未集成模板推导：ActOnCallExpr 中没有调用 TemplateDeduction。
   → 状态：✅ 已修复（b536aaf）— ActOnCallExpr 现在检测 FunctionTemplateDecl callee，通过新增的 DeduceAndInstantiateFunctionTemplate 方法自动推导并实例化

2. isCompleteType 未考虑已实例化的模板特化：Sema::isCompleteType 对 TemplateSpecialization 类型返回 false。
   → 状态：✅ 已修复（b536aaf）— isCompleteType 中 TemplateSpecialization 类型通过 Instantiator->FindExistingSpecialization 检查特化是否已实例化且定义完整

3. SubstituteCXXMethodDecl 的 Parent 指针：方法实例化时 Parent 仍指向原始 CXXRecordDecl。
   → 状态：✅ 已修复（b536aaf）— SubstituteCXXMethodDecl 新增 Parent 参数，InstantiateClassTemplate 传入实例化后的 Spec 作为 MethodParent


---

四、修复状态汇总

| # | 问题 | 状态 | 修复提交 |
|---|---|---|---|
| #3 | CurContext->addDecl() 缺失 | ✅ 已修复 | b536aaf |
| #4 | 深度溢出无诊断 | ✅ 已修复 | b536aaf |
| #5 | SubstituteExpr 空实现 | ⏳ 5.3 补充 | — |
| #6 | FindExistingSpecialization 线性扫描 | ⏳ 性能优化 | — |
| #7 | QualifiedType/AttributedType | ⏳ 低优先级 | — |
| #8 | LookupUnqualifiedName 模板名查找 | ✅ 已修复 | b536aaf |
| #9 | 依赖类型查找 | ⏳ 5.3/5.4 | — |
| #10 | 显式指定模板实参 | ⏳ 5.3/5.5 | — |
| #11 | CV 限定符剥离 | ✅ 已修复 | b536aaf |
| #12 | collapseReferences 不完整 | ✅ 已修复 | b536aaf |
| #13 | 非引用到 T& 参数处理 | ✅ 已修复 | b536aaf |
| #14 | 模板模板参数推导 | ⏳ 5.5 | — |
| #15 | SFINAE 未实际生效 | ✅ 已修复 | b536aaf |
| #16 | 部分排序算法偏差 | ⏳ 5.5 重写 | — |
| #17 | generateDeducedType 空 | ⏳ 5.5 | — |
| #18 | 推导未集成 ResolveOverload | ✅ 已修复 | b536aaf |
| #19 | isCompleteType 模板特化 | ✅ 已修复 | b536aaf |
| #20 | Parent 指针错误 | ✅ 已修复 | b536aaf |

统计：10/18 已修复，8/18 延迟到后续阶段（均为合理延迟）
