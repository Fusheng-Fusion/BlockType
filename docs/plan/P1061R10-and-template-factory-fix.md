# P1061R10 & 模板工厂函数完善计划

## 📋 当前状态分析

### 问题1: 包展开 (P1061R10) - 未实现

**目标语法**:
```cpp
template <typename... Ts>
void f(auto [...bindings] = std::make_tuple(Ts{}...)) {
    // bindings... 展开为多个BindingDecl
}
```

**现状**:
- BindingDecl已有 `IsPackExpansion` 字段但未使用
- Parser未解析 `...ns` 语法
- Sema未处理参数包展开的实例化

**工作量**: 中等 (~3-4小时)

---

### 问题2: 模板工厂函数 - 有问题

**测试文件**: `tests/cpp26/test_aggregate_and_deduction.cpp`

**错误信息**:
```
<input>: error: expected '{'
<input>: error: return type mismatch: expected '%0', got '%1'
<input>: error: type '<null>' cannot be used in structured binding
```

**根本原因**:
1. **聚合初始化不完善** - InitListExpr与ClassTemplateSpecialization的字段关联
2. **模板实参推导缺失** - 从实参类型推导模板参数（如从`42, 3.14`推导`T1=int, T2=double`）
3. **返回类型推导问题** - auto返回类型需要从return语句推导

**工作量**: 较大 (~6-8小时)

---

## 🎯 实施计划

### Phase 1: 修复模板工厂函数 (高优先级)

#### Task 1.1: 完善聚合初始化支持

**目标**: 支持 `MyPair<int, double> p{42, 3.14}` 语法

**需要修改**:
1. `src/Sema/Sema.cpp` - ActOnInitListExpr
   - 检测ClassTemplateSpecialization
   - 将InitListExpr的元素映射到特化的字段
   - 执行类型检查和隐式转换

2. `src/CodeGen/CodeGenExpr.cpp` - EmitInitListExpr
   - 生成字段初始化代码
   - 处理默认成员初始化器

**验收标准**:
- ✅ `MyPair<int, double> p{42, 3.14}` 编译通过
- ✅ 类型不匹配时给出诊断
- ✅ 支持嵌套聚合初始化

---

#### Task 1.2: 实现模板实参推导

**目标**: 从函数调用实参推导模板参数

**需要实现**:
1. `src/Sema/SemaTemplateDeduction.cpp` (新建)
   - `DeduceTemplateArguments(FunctionTemplateDecl*, ArrayRef<Expr*>)` 
   - 模式匹配：从ParmVarDecl类型推导TemplateTypeParm
   - 处理转发引用（T&&）
   - 处理数组到指针的decay

2. 集成到 `BuildCallExpr`:
   - 检测FunctionTemplateDecl
   - 调用DeduceTemplateArguments
   - 如果推导成功，实例化函数模板
   - 如果推导失败，报告诊断

**关键算法**:
```cpp
// 伪代码
for each (ParmVarDecl *Param : FuncParams) {
  QualType ParamType = Param->getType();
  Expr *Arg = CallArgs[i];
  QualType ArgType = Arg->getType();
  
  // 模式匹配
  if (ParamType is TemplateTypeParmType<T>) {
    DeducedArgs[T->getIndex()] = ArgType;
  }
  else if (ParamType is RValueReferenceType to TemplateTypeParmType<T>) {
    // 转发引用特殊处理
    DeducedArgs[T->getIndex()] = ArgType;
  }
}
```

**验收标准**:
- ✅ `make_my_pair(42, 3.14)` 推导出 `T1=int, T2=double`
- ✅ `make_my_pair('A', 100)` 推导出 `T1=char, T2=int`
- ✅ 推导失败时给出清晰诊断

---

#### Task 1.3: 完善auto返回类型推导

**目标**: 从return语句推导函数返回类型

**需要修改**:
1. `src/Sema/Sema.cpp` - CheckFunctionReturnTypes
   - 遍历函数体找到所有return语句
   - 收集返回表达式的类型
   - 统一为共同的类型（如果有多个return）
   - 替换函数的ReturnType

2. 集成到 `ActOnFunctionDeclarator`:
   - 如果返回类型是AutoType
   - 在函数定义结束时触发推导
   - 更新FunctionDecl的ReturnType

**验收标准**:
- ✅ `auto foo() { return 42; }` 推导为 `int`
- ✅ `auto bar() { return 3.14; }` 推导为 `double`
- ✅ 多个return语句类型一致

---

### Phase 2: 实现包展开 (P1061R10) (中优先级)

#### Task 2.1: Parser支持包展开语法

**目标**: 解析 `auto [...names] = expr` 语法

**需要修改**:
1. `src/Parse/ParseDecl.cpp` - parseStructuredBindingDeclaration
   - 检测标识符后的 `...`
   - 设置 `IsPackExpansion = true`
   - 存储单个名称作为包名

2. `include/blocktype/AST/Decl.h` - BindingDecl
   - 已有 `IsPackExpansion` 字段
   - 可能需要添加 `PackName` 字段

**验收标准**:
- ✅ 能解析 `auto [...args] = tuple`
- ✅ AST中正确标记IsPackExpansion

---

#### Task 2.2: Sema处理包展开

**目标**: 将包展开转换为多个BindingDecl

**需要实现**:
1. `src/Sema/Sema.cpp` - ExpandPackInDecompositionDecl
   - 获取tuple的元素数量（从模板参数或tuple_size）
   - 为每个元素创建独立的BindingDecl
   - 命名规则：args_0, args_1, ... 或使用原始包名

2. 集成到 `ActOnDecompositionDecl`:
   - 检测是否有pack expansion binding
   - 调用ExpandPackInDecompositionDecl
   - 返回展开后的DeclGroupRef

**验收标准**:
- ✅ `auto [...args] = make_tuple(1, 2.0, 'c')` 展开为3个BindingDecl
- ✅ 每个binding有正确的类型（int, double, char）

---

#### Task 2.3: CodeGen生成包展开代码

**目标**: 为展开的bindings生成正确的代码

**需要修改**:
1. `src/CodeGen/CodeGenStmt.cpp` - EmitBindingDecl
   - 已支持普通BindingDecl
   - Pack expansion的bindings已经是普通BindingDecl，无需特殊处理

**验收标准**:
- ✅ 生成的IR中每个binding有独立的alloca
- ✅ 正确调用std::get<N>提取值

---

## 📊 优先级和时间估算

| 任务 | 优先级 | 预计时间 | 依赖 |
|------|--------|----------|------|
| Task 1.1: 聚合初始化 | P0 | 2小时 | 无 |
| Task 1.2: 模板实参推导 | P0 | 4小时 | 无 |
| Task 1.3: auto返回类型推导 | P1 | 2小时 | Task 1.2 |
| Task 2.1: Parser包展开 | P2 | 1小时 | 无 |
| Task 2.2: Sema包展开 | P2 | 2小时 | Task 2.1 |
| Task 2.3: CodeGen包展开 | P2 | 0.5小时 | Task 2.2 |
| **总计** | - | **~11.5小时** | - |

---

## 🔗 相关文件清单

### 需要新建的文件
1. `src/Sema/SemaTemplateDeduction.cpp` - 模板实参推导核心逻辑
2. `include/blocktype/Sema/SemaTemplateDeduction.h` - 推导接口声明

### 需要修改的文件
1. `src/Sema/Sema.cpp` - 聚合初始化、auto返回类型推导
2. `src/Parse/ParseDecl.cpp` - 包展开语法解析
3. `src/CodeGen/CodeGenExpr.cpp` - InitListExpr代码生成
4. `include/blocktype/AST/Decl.h` - BindingDecl可能需要扩展

### 测试文件
1. `tests/cpp26/test_aggregate_and_deduction.cpp` - 现有测试，需要修复
2. `tests/cpp26/test_pack_expansion_bindings.cpp` - 新建包展开测试

---

## ✅ 验收标准

### 模板工厂函数
- [ ] `test_aggregate_and_deduction.cpp` 编译通过无错误
- [ ] 聚合初始化正确工作
- [ ] 模板实参推导正确工作
- [ ] auto返回类型推导正确工作

### 包展开 (P1061R10)
- [ ] Parser能解析 `auto [...names] = expr`
- [ ] Sema正确展开为多个BindingDecl
- [ ] CodeGen生成正确的代码
- [ ] 至少3个测试用例

---

## 🚀 建议实施顺序

**推荐顺序**:
1. **Task 1.1** - 聚合初始化（基础，独立）
2. **Task 1.2** - 模板实参推导（核心，最复杂）
3. **Task 1.3** - auto返回类型（依赖1.2）
4. **验证** - 确保test_aggregate_and_deduction.cpp通过
5. **Task 2.1-2.3** - 包展开（可选，P2优先级）

**理由**:
- 模板工厂函数问题影响现有测试，优先级更高
- 包展开是C++26新特性，可以稍后实现
- 按依赖关系逐步推进，避免返工

---

*创建日期: 2026-04-20*
*下一步: 开始Task 1.1 - 完善聚合初始化支持*
