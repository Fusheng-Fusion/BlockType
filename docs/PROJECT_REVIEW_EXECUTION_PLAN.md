# BlockType 项目全面审查执行方案

**版本**: v1.0  
**创建时间**: 2026-04-19  
**执行者**: AI助手 + 人工review  
**预计总时长**: 分多次执行，每次15-30分钟

---

## 📋 方案概述

### 审查目标
找出项目中所有"开发了但未集成到编译流程"的功能模块，并生成可执行的集成计划。

### 审查范围
- Parser 模块（src/Parse/）
- Sema 模块（src/Sema/）
- TypeCheck 模块（src/Sema/TypeCheck.*）
- CodeGen 模块（src/CodeGen/）
- AST/Type 系统（include/blocktype/AST/, src/AST/）
- 集成调用链（从 main() 到各模块的完整路径）

### 执行策略
**渐进式审查**：将大任务拆成小任务，每次执行一个，人工review后再继续。

**人机协作**：
- AI负责：自动化扫描、生成清单、初步分析
- 人负责：确认方向、判断优先级、决策修复方案

---

## 🎯 任务分解结构

```
总任务：项目全面审查
├── Phase 1: Parser 模块审查（Task 1.x）
├── Phase 2: Sema 模块审查（Task 2.x）
├── Phase 3: TypeCheck 模块审查（Task 3.x）
├── Phase 4: CodeGen 模块审查（Task 4.x）
├── Phase 5: AST/Type 系统审查（Task 5.x）
└── Phase 6: 集成调用链审查（Task 6.x）
    └── Final: 汇总报告与行动计划
```

---

## 📝 详细任务清单

---

### **Phase 1: Parser 模块审查**

#### Task 1.1: 提取所有 Parser 函数
**目标**: 列出 Parser 类的所有 public 解析函数

**执行步骤**:
```bash
# Step 1: 提取函数名
grep -rh "^Expr \*Parser::\|^Stmt \*Parser::\|^Decl \*Parser::\|^QualType Parser::\|^void Parser::parse" src/Parse/*.cpp | \
  sed 's/.*Parser:://' | sed 's/(.*$//' | sort -u > /tmp/review_parser_funcs.txt

# Step 2: 统计数量
echo "找到 $(wc -l < /tmp/review_parser_funcs.txt) 个 Parser 函数"

# Step 3: 显示前20个供验证
head -20 /tmp/review_parser_funcs.txt
```

**输出文件**: `/tmp/review_parser_funcs.txt`

**验收标准**: 
- [ ] 文件非空
- [ ] 包含预期的函数名（如 parseExpression, parseDeclaration）
- [ ] 无重复项

**预计时间**: 2分钟

---

#### Task 1.2: 检查每个函数的调用情况
**目标**: 找出未被调用的 Parser 函数

**执行步骤**:
```bash
# Step 1: 初始化结果文件
> /tmp/review_parser_unused.txt
> /tmp/review_parser_used.txt

# Step 2: 逐个检查
while IFS= read -r func; do
  # 跳过内部函数
  [[ "$func" =~ ^_ ]] && continue
  
  # 在 Parser 内部搜索调用（排除定义本身）
  count=$(grep -r "${func}(" src/Parse/*.cpp 2>/dev/null | grep -v "::${func}" | wc -l)
  
  if [ "$count" -eq 0 ]; then
    echo "$func" >> /tmp/review_parser_unused.txt
  else
    echo "$func ($count callers)" >> /tmp/review_parser_used.txt
  fi
done < /tmp/review_parser_funcs.txt

# Step 3: 统计结果
echo "=== 未使用的 Parser 函数 ==="
cat /tmp/review_parser_unused.txt
echo ""
echo "总计: $(wc -l < /tmp/review_parser_unused.txt) 个"
```

**输出文件**: 
- `/tmp/review_parser_unused.txt` - 未使用函数清单
- `/tmp/review_parser_used.txt` - 已使用函数清单

**验收标准**:
- [ ] 清单完整
- [ ] 无明显误报（如通过宏调用的函数）

**预计时间**: 5分钟

---

#### Task 1.3: 人工review游离函数
**目标**: 确认哪些是真的游离，哪些是误报

**执行人工review**:
1. 阅读 `/tmp/review_parser_unused.txt`
2. 对每个函数判断：
   - **真的游离**：确实没有被调用，应该集成或删除
   - **误报**：通过其他方式调用（如函数指针、宏、模板）
   - **预留接口**：为未来功能预留，暂时不用
3. 标记分类

**输出文件**: `/tmp/review_parser_unused_classified.txt`

**格式**:
```
# 真的游离（需要集成）
parseTrailingReturnType - 尾置返回类型，C++11功能
parseFoldExpression - 折叠表达式，C++17功能

# 误报
parseXXX - 通过宏调用

# 预留接口
parseCXX26Feature - C++26功能，暂未启用
```

**预计时间**: 10分钟（人工）

---

#### Task 1.4: 追踪关键游离函数的调用链
**目标**: 对高优先级的游离函数，分析应该在哪里集成

**执行步骤**（以 parseTrailingReturnType 为例）:
```bash
# Step 1: 查看函数实现
grep -A20 "^.*Parser::parseTrailingReturnType" src/Parse/ParseDecl.cpp

# Step 2: 查找相关语法点
grep -n "auto.*->" tests/*.cpp | head -5

# Step 3: 分析应该在何处调用
# （人工分析：应该在 parseFunctionDefinition 中检测到 "->" 时调用）
```

**输出**: 集成建议（写入 review 报告）

**预计时间**: 5分钟/函数

---

#### Task 1.5: 生成 Parser 模块审查小结
**目标**: 总结 Parser 模块的审查结果

**输出文件**: `/tmp/review_phase1_summary.md`

**内容模板**:
```markdown
## Parser 模块审查结果

### 统计
- 总函数数: X
- 已集成: Y
- 游离: Z
- 集成率: W%

### 高优先级游离功能
1. parseTrailingReturnType - 应该在 parseFunctionDefinition 中集成
2. ...

### 建议
- 立即集成: [列表]
- 后续规划: [列表]
- 可以删除: [列表]
```

**预计时间**: 5分钟

---

### **Phase 2: Sema 模块审查**

#### Task 2.1: 提取所有 Sema 函数
**目标**: 列出 Sema 类的所有 public ActOn/Check/Lookup 函数

**执行步骤**:
```bash
# Step 1: 提取函数名
grep -rh "^ExprResult Sema::\|^StmtResult Sema::\|^DeclResult Sema::\|^QualType Sema::\|^FunctionDecl \*Sema::\|^bool Sema::" src/Sema/*.cpp | \
  sed 's/.*Sema:://' | sed 's/(.*$//' | sort -u > /tmp/review_sema_funcs.txt

# Step 2: 统计
echo "找到 $(wc -l < /tmp/review_sema_funcs.txt) 个 Sema 函数"
head -20 /tmp/review_sema_funcs.txt
```

**输出文件**: `/tmp/review_sema_funcs.txt`

**预计时间**: 2分钟

---

#### Task 2.2: 检查 Parser 调用情况
**目标**: 找出未被 Parser 调用的 ActOn 函数

**执行步骤**:
```bash
> /tmp/review_sema_unused_by_parser.txt

while IFS= read -r func; do
  [[ "$func" =~ ^_ ]] && continue
  [[ "$func" =~ ^Check ]] && continue  # Check函数由Sema内部调用
  [[ "$func" =~ ^Lookup ]] && continue
  [[ "$func" =~ ^Instantiate ]] && continue
  [[ "$func" =~ ^Deduce ]] && continue
  
  # 只检查 ActOn 系列
  [[ "$func" =~ ^ActOn ]] || continue
  
  # 在 Parser 中搜索调用
  count=$(grep -r "Actions\.${func}(" src/Parse/*.cpp 2>/dev/null | wc -l)
  
  if [ "$count" -eq 0 ]; then
    echo "$func" >> /tmp/review_sema_unused_by_parser.txt
  fi
done < /tmp/review_sema_funcs.txt

echo "=== 未被Parser调用的ActOn函数 ==="
cat /tmp/review_sema_unused_by_parser.txt
echo "总计: $(wc -l < /tmp/review_sema_unused_by_parser.txt) 个"
```

**输出文件**: `/tmp/review_sema_unused_by_parser.txt`

**预计时间**: 5分钟

---

#### Task 2.3: 检查 Sema 内部调用链
**目标**: 找出整个调用链都未触发的函数

**执行步骤**:
```bash
> /tmp/review_sema_dead_code.txt

while IFS= read -r func; do
  [[ "$func" =~ ^_ ]] && continue
  
  # 在整个项目中搜索（排除定义本身）
  count=$(grep -r "${func}(" src/ 2>/dev/null | grep -v "::${func}" | wc -l)
  
  if [ "$count" -eq 0 ]; then
    echo "$func" >> /tmp/review_sema_dead_code.txt
  fi
done < /tmp/review_sema_funcs.txt

echo "=== 完全死代码 ==="
cat /tmp/review_sema_dead_code.txt
```

**输出文件**: `/tmp/review_sema_dead_code.txt`

**预计时间**: 10分钟

---

#### Task 2.4: 重点问题分析 - ActOnCallExpr unreachable code
**目标**: 深入分析第2094-2098行的逻辑错误

**执行步骤**:
```bash
# Step 1: 提取相关代码段
sed -n '2092,2130p' src/Sema/Sema.cpp > /tmp/review_actoncall_snippet.txt

# Step 2: 分析问题
cat /tmp/review_actoncall_snippet.txt

# Step 3: 追踪 DeclRefExpr(nullptr) 的来源
grep -n "DeclRefExpr.*nullptr\|ActOnDeclRefExpr.*NULL" src/Parse/*.cpp
```

**输出**: 问题根因分析报告

**预计时间**: 10分钟

---

#### Task 2.5: 生成 Sema 模块审查小结
**目标**: 总结 Sema 模块的审查结果

**输出文件**: `/tmp/review_phase2_summary.md`

**预计时间**: 5分钟

---

### **Phase 3: TypeCheck 模块审查**

#### Task 3.1: 提取所有 Check 函数
**执行步骤**:
```bash
grep -rh "^bool TypeCheck::\|^QualType TypeCheck::" src/Sema/TypeCheck.cpp | \
  sed 's/.*TypeCheck:://' | sed 's/(.*$//' | sort -u > /tmp/review_typecheck_funcs.txt

echo "找到 $(wc -l < /tmp/review_typecheck_funcs.txt) 个 TypeCheck 函数"
```

**输出文件**: `/tmp/review_typecheck_funcs.txt`

**预计时间**: 2分钟

---

#### Task 3.2: 检查 Sema 调用情况
**执行步骤**:
```bash
> /tmp/review_typecheck_unused.txt

while IFS= read -r func; do
  count=$(grep -r "TC\.${func}(" src/Sema/*.cpp 2>/dev/null | wc -l)
  if [ "$count" -eq 0 ]; then
    echo "$func" >> /tmp/review_typecheck_unused.txt
  fi
done < /tmp/review_typecheck_funcs.txt

echo "=== 未被Sema调用的Check函数 ==="
cat /tmp/review_typecheck_unused.txt
```

**输出文件**: `/tmp/review_typecheck_unused.txt`

**预计时间**: 5分钟

---

#### Task 3.3: 生成 TypeCheck 模块审查小结
**输出文件**: `/tmp/review_phase3_summary.md`

**预计时间**: 3分钟

---

### **Phase 4: CodeGen 模块审查**

#### Task 4.1: 提取所有 Emit 函数
**执行步骤**:
```bash
grep -rh "^void.*::Emit\|^llvm::Value \*.*::Emit" src/CodeGen/*.cpp | \
  sed 's/.*::Emit/Emit/' | sed 's/(.*$//' | sort -u > /tmp/review_codegen_emit_funcs.txt

echo "找到 $(wc -l < /tmp/review_codegen_emit_funcs.txt) 个 Emit 函数"
```

**输出文件**: `/tmp/review_codegen_emit_funcs.txt`

**预计时间**: 2分钟

---

#### Task 4.2: 检查 AST Visitor 覆盖度
**执行步骤**:
```bash
# Step 1: 提取所有 AST 节点类型
grep "^DECL(" include/blocktype/AST/NodeKinds.def | sed 's/DECL(\([^,]*\).*/\1/' > /tmp/review_ast_nodes.txt

# Step 2: 提取所有 Visit 函数
grep -rh "Visit[A-Z]" src/CodeGen/*.cpp | grep -oP 'Visit\K[A-Za-z]+' | sort -u > /tmp/review_codegen_visits.txt

# Step 3: 对比
echo "=== 未处理的 AST 节点 ==="
comm -23 /tmp/review_ast_nodes.txt /tmp/review_codegen_visits.txt
```

**输出文件**: 
- `/tmp/review_ast_nodes.txt`
- `/tmp/review_codegen_visits.txt`
- 未处理节点清单

**预计时间**: 5分钟

---

#### Task 4.3: 生成 CodeGen 模块审查小结
**输出文件**: `/tmp/review_phase4_summary.md`

**预计时间**: 3分钟

---

### **Phase 5: AST/Type 系统审查**

#### Task 5.1: 检查 NodeKinds 完整性
**执行步骤**:
```bash
# 检查是否有节点类型定义了但没有类实现
for kind in $(cat /tmp/review_ast_nodes.txt); do
  if ! grep -q "class ${kind}" include/blocktype/AST/*.h; then
    echo "MISSING_CLASS: $kind"
  fi
done
```

**输出**: 缺失实现的节点类型清单

**预计时间**: 3分钟

---

#### Task 5.2: 检查 TypeVisitor 覆盖度
**执行步骤**:
```bash
# 提取所有 TypeClass
grep "TYPE(" include/blocktype/AST/Type.h | grep -oP 'TYPE\(\K[A-Za-z]+' > /tmp/review_type_classes.txt

# 提取 TypeVisitor 处理方法
grep -rh "Visit[A-Z].*Type" src/AST/*.cpp include/blocktype/AST/*.h | \
  grep -oP 'Visit\K[A-Za-z]+(?=Type)' | sort -u > /tmp/review_type_visits.txt

# 对比
echo "=== 未处理的 Type 类别 ==="
comm -23 /tmp/review_type_classes.txt /tmp/review_type_visits.txt
```

**输出**: 未处理的 Type 类别清单

**预计时间**: 3分钟

---

#### Task 5.3: 生成 AST/Type 系统审查小结
**输出文件**: `/tmp/review_phase5_summary.md`

**预计时间**: 3分钟

---

### **Phase 6: 集成调用链审查**

#### Task 6.1: 绘制主调用链
**目标**: 从 main() 开始追踪完整调用路径

**执行步骤**（人工+工具）:
```bash
# Step 1: 找到 main 函数
grep -rn "^int main\|^main(" tools/*.cpp src/*.cpp

# Step 2: 追踪 Driver::compile
grep -A30 "Driver::compile" src/Driver/Driver.cpp | head -40

# Step 3: 追踪 Parser 入口
grep -n "parseTranslationUnit" src/Parse/*.cpp

# Step 4: 串联成调用图
```

**输出**: Mermaid 格式的调用图

**预计时间**: 10分钟

---

#### Task 6.2: 检测断裂点
**目标**: 找出调用链中的断点

**执行步骤**:
对每个关键节点，检查：
1. 返回值是否被处理？
2. 错误路径是否完整？
3. 是否有 early return 导致后续代码 unreachable？

**重点检查**:
- ActOnCallExpr L2094-2098
- 其他已知的早期返回点

**输出**: 断裂点清单

**预计时间**: 15分钟

---

#### Task 6.3: 生成集成调用链审查小结
**输出文件**: `/tmp/review_phase6_summary.md`

**预计时间**: 5分钟

---

### **Final: 汇总报告与行动计划**

#### Task F.1: 合并所有阶段的结果
**执行步骤**:
```bash
cat /tmp/review_phase{1,2,3,4,5,6}_summary.md > /tmp/review_full_report_draft.md
```

**输出**: 完整审查报告草稿

**预计时间**: 5分钟

---

#### Task F.2: 生成优先级矩阵
**目标**: 按影响范围和修复难度排序

**输出表格**:
| 问题 | 影响范围 | 修复难度 | 优先级 | 预计工作量 |
|------|---------|---------|--------|-----------|
| ActOnCallExpr unreachable | 函数模板调用 | 中 | P0 | 2h |
| parseTrailingReturnType | 尾置返回类型 | 低 | P1 | 1h |
| ... | ... | ... | ... | ... |

**预计时间**: 10分钟

---

#### Task F.3: 制定集成计划
**输出**: 
- 短期计划（本周）
- 中期计划（本月）
- 长期计划（季度）

**预计时间**: 10分钟

---

#### Task F.4: 生成最终报告
**输出文件**: `docs/PROJECT_REVIEW_FINAL_REPORT.md`

**内容结构**:
```markdown
# BlockType 项目全面审查报告

## 1. 执行摘要
## 2. 各模块审查结果
## 3. 关键问题分析
## 4. 优先级矩阵
## 5. 集成计划
## 附录：详细清单
```

**预计时间**: 15分钟

---

## 📊 总工作量估算

| Phase | 任务数 | 预计时间 | 累计时间 |
|-------|--------|---------|---------|
| Phase 1: Parser | 5 | 27分钟 | 27分钟 |
| Phase 2: Sema | 5 | 32分钟 | 59分钟 |
| Phase 3: TypeCheck | 3 | 10分钟 | 69分钟 |
| Phase 4: CodeGen | 3 | 10分钟 | 79分钟 |
| Phase 5: AST/Type | 3 | 9分钟 | 88分钟 |
| Phase 6: 集成调用链 | 3 | 30分钟 | 118分钟 |
| Final: 汇总 | 4 | 40分钟 | 158分钟 |
| **总计** | **26** | **~2.5小时** | - |

**注意**: 这是纯执行时间，不包括人工review时间。实际完成可能需要分多次进行。

---

## ✅ 执行规则

### 规则1: 每次只执行一个 Task
- 完成一个 Task 后，暂停
- 输出结果给人工review
- 确认无误后，再执行下一个 Task

### 规则2: 遇到异常立即停止
- 如果某个 Task 执行失败或结果异常
- 不要继续，先分析问题
- 调整方案后再继续

### 规则3: 保持输出文件的一致性
- 所有中间结果保存到 `/tmp/review_*.txt`
- 所有阶段小结保存到 `/tmp/review_phase*_summary.md`
- 最终报告保存到 `docs/PROJECT_REVIEW_FINAL_REPORT.md`

### 规则4: 人工review关键点
以下 Task 完成后必须人工review：
- Task 1.3（Parser 游离函数分类）
- Task 2.4（ActOnCallExpr 问题分析）
- Task 6.2（断裂点检测）
- Task F.2（优先级矩阵）

---

## 🚀 开始执行

**现在准备开始执行 Task 1.1。**

请确认：
1. 这个方案是否合理？
2. 是否需要调整某些 Task 的顺序或内容？
3. 是否可以开始执行 Task 1.1？

**等待你的确认后，我将开始执行 Task 1.1。**

