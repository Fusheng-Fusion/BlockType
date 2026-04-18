Audit Findings
## A. 对比开发文档 (06-PHASE6-irgen.md) — Stage 6.1 部分

## -----------------------------------------------------------

## Task 6.1.1 CodeGenModule 类
文档要求 vs 实际实现：

1.✅ EmitTranslationUnit(TranslationUnitDecl *TU) — 已实现
--✅ [已修复] 两遍发射逻辑已修正：
----第一遍：声明（函数声明 + 全局变量延迟队列 + Record 前向声明 + 类布局计算）
----EmitDeferred：发射全局变量定义
----第二遍（新增）：遍历 TU->decls() 调用 EmitFunction 生成有函数体的函数
----最后：发射全局构造/析构

2.✅ EmitDeferred() — 已实现，逻辑已简化
--✅ [已修复] 移除了无效的 Module 函数遍历循环，只负责发射延迟全局变量

3.✅ EmitGlobalVar(VarDecl *VD) — 已实现
--✅ [已修复] isConstant 判断现在使用 isConstexpr() || isConstQualified()
--✅ [已修复] static 变量使用 InternalLinkage，其他使用 ExternalLinkage
--✅ 注册到 GlobalValues 表正确

4.✅ GetGlobalVar(VarDecl *VD) — 已实现

5.✅ EmitFunction(FunctionDecl *FD) — 已实现

6.✅ GetFunction(FunctionDecl *FD) — 已实现

7.✅ GetOrCreateFunctionDecl(FunctionDecl *FD) — 已实现
--✅ [已修复] inline 函数使用 LinkOnceODRLinkage，并添加 AlwaysInline 属性
--✅ [已修复] noexcept(true) 函数设置 DoesNotThrow 属性

8.✅ EmitVTable(CXXRecordDecl *RD) — 委托到 CGCXX

9.✅ EmitClassLayout(CXXRecordDecl *RD) — 委托到 CGCXX

10.✅ AddGlobalCtor/AddGlobalDtor — 已实现

11.✅ EmitGlobalCtorDtors() — 已实现

## -----------------------------------------------------------
## Task 6.1.2 CodeGenTypes 类型映射
文档要求 vs 实际实现：

1.✅ ConvertType(QualType) — 已实现，分派逻辑完整
--所有 TypeClass 都覆盖了

2.✅ ConvertTypeForMem(QualType) — 已实现（简单委托）

3.✅ [已修复] ConvertTypeForValue(QualType) — 已新增实现
--数组类型的值类型返回指向首元素的指针（LLVM 中数组不能作为一等值传递）
--其他类型委托给 ConvertType

4.✅ GetFunctionType(const FunctionType *) — 已实现

5.✅ GetFunctionTypeForDecl(FunctionDecl *) — 已实现
--✅ [已修复] 成员函数 this 指针已在函数类型中添加
--✅ [已修复] 新增 FunctionTypeCache 缓存避免重复计算
--非静态成员函数：this 指针作为第一个参数（指向类结构体的指针）

6.✅ GetRecordType(RecordDecl *) — 已实现
--✅ 使用 opaque struct 先占位避免递归
--✅ CXXRecordDecl 的 vptr 处理

7.✅ [已修复] GetCXXRecordType(CXXRecordDecl *RD) — 已新增实现
--委托给 GetRecordType（已处理 vptr）

8.✅ GetFieldIndex(FieldDecl *) — 已实现
--⚠️ 实现通过遍历 RecordTypeCache，这是 O(n) 的，因为 FieldDecl 没有父指针
--效率低但功能正确（P2 待改进）

9.✅ GetTypeSize/GetTypeAlign — 委托到 TargetInfo

10.✅ [已修复] GetSize(uint64_t) / GetAlign(uint64_t) — 已新增实现
--返回 llvm::ConstantInt（i64 类型）

11✅ 所有 22 种 BuiltinType 映射 — 已实现
--⚠️ WChar 映射到 i32（平台相关，AArch64/x86_64 macOS 上是 i32，正确）
--⚠️ LongDouble 映射到 FP128 — macOS 上应为 double（P2 平台相关问题）

12✅ ConvertBuiltinType — 所有 22 种覆盖

13✅ ConvertPointerType — 已实现

14✅ ConvertReferenceType — 已实现

15✅ ConvertArrayType — 已实现

16✅ ConvertFunctionType — 已实现

17✅ ConvertRecordType — 已实现

18✅ ConvertEnumType — 已实现
--✅ [已修复] 不再硬编码 i32，现在查询 EnumDecl::getUnderlyingType()

19✅ ConvertTypedefType — 已实现

20✅ ConvertTemplateSpecializationType — 已实现

21✅ ConvertMemberPointerType — 已实现（简化为 i8*）

22✅ ConvertAutoType — 已实现

23✅ ConvertDecltypeType — 已实现

24✅ ElaboratedType 处理 — 已实现（递归到 getNamedType()）
## -----------------------------------------------------------

## Task 6.1.3 CodeGenConstant 常量生成
文档要求 vs 实际实现：

1.✅ EmitConstant(Expr *) — 已实现

2.✅ EmitConstantForType(Expr *, QualType) — 已实现

3.✅ EmitIntLiteral(IntegerLiteral *) — 已实现

4.✅ EmitFloatLiteral(FloatingLiteral *) — 已实现

5.✅ EmitStringLiteral(StringLiteral *) — 已实现

6.✅ EmitBoolLiteral(bool) — 已实现

7.✅ EmitCharLiteral(CharacterLiteral *) — 已实现

8.✅ EmitNullPointer(QualType) — 已实现

9.✅ EmitInitListExpr(InitListExpr *) — 已实现

10.✅ EmitZeroValue(QualType) — 已实现

11.✅ EmitUndefValue(QualType) — 已实现

12.✅ [已修复] EmitIntCast(llvm::Constant *, QualType, QualType) — 已新增实现
--整数截断/符号扩展

13.✅ [已修复] EmitFloatToIntCast(llvm::Constant *, QualType, QualType) — 已新增实现
--APFloat → APSInt 转换

14.✅ [已修复] EmitIntToFloatCast(llvm::Constant *, QualType, QualType) — 已新增实现
--APInt → APFloat 转换

15.✅ [已修复] GetNullPointer(QualType) — 已新增实现
--返回 ConstantPointerNull

16.✅ GetIntZero/GetIntOne — 已实现

17.✅ getLLVMContext() — 已实现

## -----------------------------------------------------------

## Task 6.1.4 TargetInfo 目标平台信息
文档要求 vs 实际实现：

✅ TargetInfo(llvm::StringRef) — 已实现
✅ getTypeSize(QualType) — 已实现
✅ getTypeAlign(QualType) — 已实现
✅ getBuiltinSize(BuiltinKind) — 已实现
✅ getBuiltinAlign(BuiltinKind) — 已实现
✅ getPointerSize() — 已实现
✅ getPointerAlign() — 已实现
✅ getDataLayout() — 已实现
✅ getTriple() — 已实现
✅ isStructReturnInRegister(QualType) — 已实现
✅ isThisPassedInRegister() — 已实现
✅ getEnumSize() — 已实现
TargetInfo 对比文档非常完整，没有遗漏。
## -----------------------------------------------------------
## B. 对比 Clang 同模块特性
## -----------------------------------------------------------

## CodeGenModule 对比 Clang CodeGenModule

1. Mangle 系统 — Clang 使用 NameMangler 生成唯一符号名（如 _Z3fooi）。BlockType 直接使用 Decl 名称，会导致：
--✅ 简化处理可接受（Stage 6.1 基础设施）
--⚠️ 后续需要支持 C++ name mangling（重载函数区分）

2. 属性处理 — Clang 处理 visibility, dllimport/dllexport, weak 等属性。BlockType 未处理任何函数/变量属性。
--P2 问题：缺少属性处理框架

3. Linkage/Visibility — Clang 正确处理 InternalLinkage/ExternalLinkage/WeakAnyLinkage 等。
--✅ [已修复] static 全局变量使用 InternalLinkage
--✅ [已修复] inline 函数使用 LinkOnceODRLinkage
--⚠️ 完整的 Visibility 属性未处理（P2）

4. 延迟函数发射 — Clang 有完整的延迟发射机制（deferred functions）。
--✅ [已修复] 两遍发射逻辑已修正，函数体正确发射

5. 全局变量初始化优先级 — Clang 区分常量初始化（可编译期完成）和动态初始化。BlockType 未区分。
--P2 问题

## -----------------------------------------------------------


## CodeGenTypes 对比 Clang CodeGenTypes

1. 函数类型生成 — Clang 的 GetFunctionType 处理了：
--✅ [已修复] this 指针（成员函数首个参数）已处理
--⚠️ sret 参数（结构体返回值通过内存传递）未处理 — P2
--⚠️ inreg 属性未处理 — P2
--✅ 变参处理已实现

2. 结构体布局 — Clang 使用 ASTContext::getASTRecordLayout 获取精确布局（含基类子对象、虚基类、填充）。BlockType 简化为按声明顺序排列。
--✅ 基础设施阶段可接受

3. 枚举类型 — Clang 正确获取枚举的底层类型（通过 EnumDecl::getIntegerType()）。
--✅ [已修复] 现在查询 EnumDecl::getUnderlyingType()

4. Record 类型缺少基类子对象 — BlockType 的 GetRecordType 不处理基类字段。
--P2 问题：多重/单一继承布局未处理

## -----------------------------------------------------------

## CodeGenConstant 对比 Clang ConstantEmitter

1. 常量折叠 — Clang 在 EmitConstant 中处理了更多常量表达式类型（sizeof、alignof、地址常量等）。BlockType 处理了基本类型。
--⚠️ 缺少 sizeof/alignof 常量表达式（P2）

2. 字符串字面量合并 — Clang 会合并相同的字符串字面量。BlockType 每次创建新的全局变量。
--P2 问题

3. 静态初始化 — Clang 区分静态初始化和动态初始化。BlockType 未区分。
--P2 问题

## -----------------------------------------------------------

## CGCXX 对比 Clang CGCXX + CGClass + CGVTables
1. VTable 布局 — Clang 的 VTable 布局包含：
--offset-to-top（偏移到顶部）
--RTTI 指针
--虚函数指针（含覆盖方法）
BlockType 简化为只有 RTTI + 虚函数指针。
--⚠️ 缺少 offset-to-top（P2）

2. VTable 继承 — Clang 正确处理派生类继承基类的 VTable。BlockType 未处理。
--P2 问题

3. 虚析构函数 — Clang 为虚析构函数生成 deleting/complete 两个版本。BlockType 未处理。
--P2 问题

## -----------------------------------------------------------

C. 关联关系错误
## -----------------------------------------------------------

1. ✅ [已修复] EmitTranslationUnit 的两遍发射逻辑已修正：
--第一遍：创建声明 + 延迟全局变量 + 类布局
--EmitDeferred：发射全局变量定义
--第二遍（新增）：遍历 TU->decls() 调用 EmitFunction 生成有函数体的函数

2. ✅ [已修复] CodeGenTypes.h 已添加 FunctionTypeCache
--在 GetFunctionTypeForDecl 中使用缓存

3. ✅ PointerType::getPointeeType() 返回 const Type* — 代码中使用 QualType(PT->getPointeeType(), Qualifier::None) 正确

4. ⚠️ TargetInfo 构造函数使用 DataLayout(StringRef) — 如果传入无效的三元组，DataLayout 会是空的（P2）


## ---------------------------------------------------------------
## D. 汇总

## P0 问题（必须立即修复） — 全部已修复 ✅
1. ✅ 函数体从未被发射 — 已修正两遍发射逻辑，第二遍遍历 AST 调用 EmitFunction

## P1 问题（应尽快修复） — 全部已修复 ✅
1. ✅ 缺少 ConvertTypeForValue() — 已新增
2. ✅ 缺少 GetCXXRecordType() — 已新增
3. ✅ 缺少 GetSize() / GetAlign() — 已新增
4. ✅ 缺少 EmitIntCast / EmitFloatToIntCast / EmitIntToFloatCast — 已新增
5. ✅ 缺少 GetNullPointer() — 已新增
6. ✅ 全局变量/函数 linkage 类型判断 — static→InternalLinkage, inline→LinkOnceODR
7. ✅ 成员函数 this 指针处理 — GetFunctionTypeForDecl 中为非静态成员函数添加 this
8. ✅ 枚举类型不再硬编码 — 查询 EnumDecl::getUnderlyingType()

## P2 问题（后续改进）
1. ✅ [已修复] 函数属性设置（inline→AlwaysInline, noexcept→DoesNotThrow）
2. ✅ [已修复] 全局变量 isConstant 判断（isConstexpr || isConstQualified）
3. 字符串字面量未合并
4. VTable 布局缺少 offset-to-top
5. Record 类型布局未处理基类子对象
6. LongDouble 在 macOS 上应映射为 double 而非 FP128
7. GetFieldIndex 效率低（遍历所有 RecordTypeCache）
8. ✅ [已修复] FunctionTypeCache 已添加
