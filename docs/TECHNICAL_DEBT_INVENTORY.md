# Phase 1-3 技术债务清单

> **审计日期：** 2026-04-16
> **审计范围：** Phase 1 (Lexer) + Phase 2 (Expression Parser) + Phase 3 (Declaration Parser)
> **审计目标：** 全面清查所有简化实现、临时方案、未完成功能
> **严重程度：** 🔴 高 | 🟡 中 | 🟢 低

---

## 📊 技术债务统计

| 严重程度 | 数量 | 优先级 | 预计修复时间 |
|---------|------|--------|-------------|
| 🔴 高 | 12 | 必须在 Phase 4 前完成 | 20-30 小时 |
| 🟡 中 | 18 | 建议在 Phase 4 中完成 | 15-20 小时 |
| 🟢 低 | 15 | 可在后续版本完成 | 10-15 小时 |
| **总计** | **45** | | **45-65 小时** |

---

## 🔴 高优先级技术债务（必须修复）

### 1. NFC 规范化未完整实现

**位置：** `src/Unicode/Normalization.cpp`

**问题：**
```cpp
// TODO: Implement full NFC normalization
// For now, just re-encode the code points
// This is correct for most common cases (no combining marks)

// TODO: Implement NFC composition
// For now, return the code point as-is

// TODO: Check if code point is in NFC form
// For now, assume all code points are in NFC
```

**影响：**
- ❌ 无法正确处理组合字符（如 é = e + ́）
- ❌ 标识符比较可能失败
- ❌ 不符合 C++ 标准的标识符规范化要求

**修复方案：**
1. 实现完整的 NFC 分解和组合
2. 使用 Unicode 规范化表
3. 或者集成 utf8proc/ICU 库

**预计时间：** 8-12 小时

---

### 2. 模板特化与比较表达式的启发式判断

**位置：** `src/Parse/ParseExpr.cpp:685-714`

**问题：**
```cpp
// Use a heuristic: look ahead to see if this looks like template arguments
// Template arguments typically start with:
// - Type keywords (int, float, etc.)
// - Identifiers that might be types
// - Constants
// - Other template specializations (nested)

// If next token is a type keyword, it's likely a template argument
if (NextTok.is(TokenKind::kw_void) || NextTok.is(TokenKind::kw_bool) || ...) {
  return parseTemplateSpecializationExpr(Loc, Name);
}

// Otherwise, treat as comparison to be safe
// This might miss some template specializations like Vector<T>
```

**影响：**
- ⚠️ `Vector<T>` 可能被误识别为比较表达式
- ⚠️ 依赖类型参数的模板特化可能解析错误

**修复方案：**
1. 在语义分析阶段验证 `T` 是否是已知类型
2. 使用 tentative parsing（试探性解析）和回溯
3. 维护已知模板的符号表

**预计时间：** 4-6 小时

---

### 3. 参数包展开未完整标记

**位置：** `src/Parse/ParseDecl.cpp:1492-1522`

**问题：**
```cpp
// Parse the pack pattern (could be a type or expression)
// For now, we just parse it as a type or expression
// In a real compiler, this would be marked as a pack expansion

// Note: We should mark this as a pack expansion
// For now, we just return the pattern

// Mark as pack expansion (for now, just return the type)
```

**影响：**
- ❌ 无法区分 `Ts...` 和普通类型
- ❌ 无法进行参数包展开
- ❌ 阻塞可变参数模板实例化

**修复方案：**
1. 在 `TemplateArgument` 中添加 `IsPackExpansion` 标志
2. 在 AST 中正确标记参数包
3. 实现参数包展开逻辑

**预计时间：** 3-4 小时

---

### 4. 枚举底层类型未存储

**位置：** `src/Parse/ParseDecl.cpp:2362-2364`

**问题：**
```cpp
// Note: We should store the underlying type in EnumDecl
// For now, we just parse it and discard
```

**影响：**
- ❌ 无法支持强类型枚举（enum class）
- ❌ 无法进行枚举大小计算
- ❌ 语义分析信息不完整

**修复方案：**
1. 在 `EnumDecl` 中添加 `UnderlyingType` 字段
2. 存储解析的底层类型
3. 提供访问方法

**预计时间：** 1-2 小时

---

### 5. 属性解析不完整

**位置：** `src/Parse/ParseDecl.cpp:2128-2130`

**问题：**
```cpp
// Store the attribute (will be added to ModuleDecl later)
// For now, we just skip it
```

**影响：**
- ⚠️ 模块属性丢失
- ⚠️ 无法支持模块属性语义

**修复方案：**
1. 解析属性名称和参数
2. 存储到 `ModuleDecl` 或其他声明
3. 支持常见属性

**预计时间：** 2-3 小时

---

### 6. 模板模板参数约束未存储

**位置：** `src/Parse/ParseDecl.cpp:1406-1408`

**问题：**
```cpp
// Note: TemplateTemplateParmDecl should have a constraint field
// For now, we just parse and discard
```

**影响：**
- ❌ 无法支持模板模板参数约束
- ❌ C++20 约束功能不完整

**修复方案：**
1. 在 `TemplateTemplateParmDecl` 添加约束字段
2. 存储约束表达式
3. 支持约束检查

**预计时间：** 2-3 小时

---

### 7. 数组类型创建简化

**位置：** `src/AST/ASTContext.cpp:72-75`

**问题：**
```cpp
// For now, create an incomplete array type
// This method is kept for compatibility with existing code
if (Size) {
  // Try to evaluate the size expression
  // For now, create an incomplete array type
  return getIncompleteArrayType(Element);
}
```

**影响：**
- ⚠️ 无法区分 `int[10]` 和 `int[]`
- ⚠️ 数组大小信息丢失
- ⚠️ 阻塞数组类型检查

**修复方案：**
1. 实现常量表达式求值
2. 正确创建 `ConstantArrayType`
3. 支持 VLA（变长数组）

**预计时间：** 3-4 小时

---

### 8. 成员访问查找简化

**位置：** `src/Parse/ParseExpr.cpp:67`

**问题：**
```cpp
// For now, we do a simple linear search through fields
// A full implementation would use a proper symbol table
```

**影响：**
- ⚠️ 性能低下（O(n) 查找）
- ⚠️ 无法处理继承成员
- ⚠️ 无法处理访问控制

**修复方案：**
1. 实现完整的符号表
2. 支持继承成员查找
3. 支持访问控制检查

**预计时间：** 4-5 小时

---

### 9. 浮点数转换错误忽略

**位置：** `src/Parse/ParseExpr.cpp:559`

**问题：**
```cpp
// Ignore conversion errors for now
if (Result) {
  // Conversion failed
}
```

**影响：**
- ⚠️ 无效浮点数不报错
- ⚠️ 可能创建错误的 AST 节点

**修复方案：**
1. 检查转换结果
2. 发出适当的诊断信息
3. 创建错误恢复表达式

**预计时间：** 1 小时

---

### 10. 命名空间别名解析简化 ✅ 已修复

**位置：** `src/Parse/ParseDecl.cpp:1701-1748`

**问题：**
```cpp
// Note: We've already consumed the alias name, so we need to handle it
// Actually, we can just call parseNamespaceAlias with the current state
// But parseNamespaceAlias expects to start after 'namespace' keyword
```

**影响：**
- ⚠️ 命名空间别名可能解析错误
- ⚠️ 代码逻辑不清晰

**修复方案：**
1. 重构 `parseNamespaceAlias` 接口
2. 正确处理已消耗的 token
3. 添加测试用例

**预计时间：** 1-2 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 重构了 `parseNamespaceAlias` 函数，添加可选参数接受已解析的别名名称
- 清理了冗余注释，代码逻辑更清晰
- 所有测试通过

---

### 11. YAML 配置解析未实现 ✅ 已修复

**位置：** `src/Basic/Translation.cpp:15-16`

**问题：**
```cpp
// TODO: 实现 YAML 解析
// 当前版本使用简化实现
```

**影响：**
- ❌ 无法加载 YAML 配置文件
- ❌ 配置管理功能不完整

**修复方案：**
1. 集成 YAML 解析库（yaml-cpp）
2. 实现配置文件加载
3. 支持配置验证

**预计时间：** 3-4 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 使用 LLVM YAMLParser 实现了完整的 YAML 解析
- 支持加载 diagnostics/en-US.yaml 和 diagnostics/zh-CN.yaml
- 添加了 TranslationManager 测试用例
- 所有测试通过

---

### 12. Framework 搜索简化 ✅ 已修复

**位置：** `src/Lex/HeaderSearch.cpp:140, 174`

**问题：**
```cpp
// Simplified canonical path - just return the filename
// Simplified implementation
```

**影响：**
- ⚠️ Framework 搜索可能不完整
- ⚠️ macOS/iOS 开发支持受限

**修复方案：**
1. 实现完整的 Framework 搜索路径
2. 支持 `.framework` 目录结构
3. 支持 Framework 版本化

**预计时间：** 2-3 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 实现了完整的路径规范化（处理 `.`、`..`、多余分隔符）
- 增强了 Framework 搜索，支持：
  - 标准 Headers 目录
  - 私有 PrivateHeaders 目录
  - 版本化 Framework (Versions/A, Versions/Current)
  - 自动添加 .h 扩展名
- 添加了路径规范化测试用例
- 所有测试通过

---

## 🟡 中优先级技术债务（建议修复）

### 13. 声明语句检测启发式 ✅ 已修复

**位置：** `src/Parse/ParseStmt.cpp:556`

**问题：**
```cpp
// Heuristic: check if we see pattern: auto? identifier : ...
```

**影响：**
- ⚠️ 可能误判复杂声明
- ⚠️ 依赖启发式而非语法规则

**修复方案：**
使用更可靠的声明检测方法

**预计时间：** 1-2 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 使用试探性解析（TentativeParsingAction）检测 range-based for
- 尝试解析类型和标识符，检查是否为 `:` 确认
- 避免启发式误判，更准确可靠
- 所有测试通过

---

### 14. 模板参数启发式判断 ✅ 已修复

**位置：** `src/Parse/ParseDecl.cpp:1503`

**问题：**
```cpp
// For now, we use a simple heuristic:
// - If it starts with a type keyword (int, float, etc.) or
//   an identifier followed by '<', it's likely a type
// - Otherwise, parse as an expression
```

**影响：**
- ⚠️ 可能误判模板参数类型
- ⚠️ 依赖启发式而非语义分析

**修复方案：**
在语义分析阶段验证参数类型

**预计时间：** 2-3 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 使用试探性解析代替启发式方法
- 先尝试解析为类型，成功则提交
- 失败则回溯并解析为表达式
- 更准确，避免误判
- 所有测试通过

---

### 15. 范围 for 语句简化 ✅ 已修复

**位置：** `src/Parse/ParseStmt.cpp:573`

**问题：**
```cpp
// Parse declaration (simplified: just auto/var name)
```

**影响：**
- ⚠️ 不支持复杂的范围声明
- ⚠️ 功能受限

**修复方案：**
完整实现范围 for 声明解析

**预计时间：** 2-3 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 使用完整的 `parseType()` 解析类型
- 支持 const、volatile、引用等限定符
- 支持 `const auto&`、`std::string&` 等复杂声明
- 所有测试通过

---

### 16. AST dump 信息不完整 ✅ 已修复

**位置：** `src/AST/Type.cpp:322, 379, 383`

**问题：**
```cpp
// TODO: Print actual parameter name from Decl
// TODO: Check template arguments
// TODO: Check expression
```

**影响：**
- ⚠️ AST dump 信息不完整
- ⚠️ 调试信息缺失

**修复方案：**
完善 AST dump 实现

**预计时间：** 1-2 小时

**修复状态：** ✅ 已完成 (2026-04-16)
- 实现 `TemplateTypeParmType::dump()` - 打印参数名称
- 实现 `RecordType::dump()` - 打印 record 名称
- 实现 `EnumType::dump()` - 打印 enum 名称
- 实现 `TypedefType::dump()` - 打印 typedef 名称
- 完善 `Type::isDependentType()` - 检查模板参数和表达式依赖性
- 所有测试通过

---

### 17. AST 访问器未实现

**位置：** `include/blocktype/AST/ASTVisitor.h:61`

**问题：**
```cpp
// TODO: Handle Decl once it is defined
```

**影响：**
- ⚠️ AST 访问器不完整
- ⚠️ 无法遍历所有 AST 节点

**修复方案：**
实现完整的 AST 访问器

**预计时间：** 2-3 小时

---

### 18. 枚举类型整数判断

**位置：** `include/blocktype/AST/Type.h:789`

**问题：**
```cpp
// TODO: Enum types are also integer types
```

**影响：**
- ⚠️ 枚举类型判断不正确
- ⚠️ 类型检查可能失败

**修复方案：**
在 `isIntegerType()` 中包含枚举类型

**预计时间：** 0.5 小时

---

### 19. Record/Enum/Typedef 名称打印

**位置：** `src/AST/Type.cpp:215, 224, 233`

**问题：**
```cpp
// TODO: Print record name once RecordDecl is defined
// TODO: Print enum name once EnumDecl is defined
// TODO: Print typedef name once TypedefNameDecl is defined
```

**影响：**
- ⚠️ 类型 dump 信息不完整
- ⚠️ 调试困难

**修复方案：**
实现名称打印

**预计时间：** 1 小时

---

### 20. 声明引用未实现

**位置：** `src/AST/Stmt.cpp:60, 200, 231`

**问题：**
```cpp
// TODO: Uncomment once Decl is defined
```

**影响：**
- ⚠️ 语句 AST 节点信息不完整
- ⚠️ 无法引用声明

**修复方案：**
添加声明引用字段

**预计时间：** 1-2 小时

---

### 21. 流式输出未实现

**位置：** `src/AI/ClaudeProvider.cpp:204`

**问题：**
```cpp
// 简化实现：调用非流式版本
```

**影响：**
- ⚠️ 无法实时获取 AI 响应
- ⚠️ 用户体验受限

**修复方案：**
实现真正的流式输出

**预计时间：** 3-4 小时

---

### 22. Unicode 规范化简化

**位置：** `src/Basic/Unicode.cpp:23`

**问题：**
```cpp
return Input.str(); // 简化实现
```

**影响：**
- ⚠️ Unicode 字符串未规范化
- ⚠️ 可能导致标识符比较失败

**修复方案：**
调用完整的 NFC 规范化

**预计时间：** 1 小时

---

### 23. 翻译配置简化

**位置：** `src/Basic/Translation.cpp:16`

**问题：**
```cpp
// 当前版本使用简化实现
```

**影响：**
- ⚠️ 配置管理功能受限

**修复方案：**
实现完整配置加载

**预计时间：** 2-3 小时

---

### 24-30. 其他中优先级问题

详见代码中的 `// For now` 注释

---

## 🟢 低优先级技术债务（可延后）

### 31. 标识符表达式处理

**位置：** `src/Parse/ParseStmt.cpp:72`

**问题：**
```cpp
return false; // For now, treat identifiers as expressions
```

**影响：**
- 可能误判标识符用途

**预计时间：** 1 小时

---

### 32. 模板参数索引临时值

**位置：** `src/Parse/ParseDecl.cpp:483`

**问题：**
```cpp
// Use 0 as the index for now (will be set correctly later)
```

**影响：**
- 模板参数索引不正确

**预计时间：** 1 小时

---

### 33. Using 声明简化

**位置：** `src/Parse/ParseDecl.cpp:715`

**问题：**
```cpp
// For now, create a UsingDecl to represent it
```

**影响：**
- Using 声明可能不完整

**预计时间：** 1-2 小时

---

### 34. 错误处理简化

**位置：** `src/Parse/ParseDecl.cpp:783`

**问题：**
```cpp
// For now, emit an error
```

**影响：**
- 错误信息可能不精确

**预计时间：** 0.5 小时

---

### 35-45. 其他低优先级问题

详见代码中的 `// For now`、`// just`、`// skip` 等注释

---

## 📋 修复优先级建议

### 第一周（Week 1）：高优先级技术债务

| 任务 | 预计时间 | 负责人 | 状态 |
|------|---------|--------|------|
| NFC 规范化完整实现 | 8-12 小时 | TBD | ⏳ 待开始 |
| 模板特化判断改进 | 4-6 小时 | TBD | ⏳ 待开始 |
| 参数包展开标记 | 3-4 小时 | TBD | ⏳ 待开始 |
| 枚举底层类型存储 | 1-2 小时 | TBD | ⏳ 待开始 |
| 属性解析完善 | 2-3 小时 | TBD | ⏳ 待开始 |
| **小计** | **18-27 小时** | | |

### 第二周（Week 2）：高优先级技术债务（续）

| 任务 | 预计时间 | 负责人 | 状态 |
|------|---------|--------|------|
| 模板模板参数约束 | 2-3 小时 | TBD | ⏳ 待开始 |
| 数组类型创建改进 | 3-4 小时 | TBD | ⏳ 待开始 |
| 成员访问查找优化 | 4-5 小时 | TBD | ⏳ 待开始 |
| 浮点数转换错误处理 | 1 小时 | TBD | ⏳ 待开始 |
| YAML 配置解析 | 3-4 小时 | TBD | ⏳ 待开始 |
| **小计** | **13-17 小时** | | |

### 第三周（Week 3）：中优先级技术债务

| 任务 | 预计时间 | 负责人 | 状态 |
|------|---------|--------|------|
| 声明语句检测改进 | 1-2 小时 | TBD | ⏳ 待开始 |
| 模板参数判断改进 | 2-3 小时 | TBD | ⏳ 待开始 |
| 范围 for 完整实现 | 2-3 小时 | TBD | ⏳ 待开始 |
| AST dump 完善 | 2-3 小时 | TBD | ⏳ 待开始 |
| AST 访问器实现 | 2-3 小时 | TBD | ⏳ 待开始 |
| 流式输出实现 | 3-4 小时 | TBD | ⏳ 待开始 |
| **小计** | **12-18 小时** | | |

---

## 🎯 技术债务清理目标

### 短期目标（Week 1-2）

- ✅ 清除所有 🔴 高优先级技术债务
- ✅ 确保 Phase 4 语义分析不受阻塞
- ✅ 提升代码质量和可维护性

### 中期目标（Week 3）

- ✅ 清除大部分 🟡 中优先级技术债务
- ✅ 完善功能实现
- ✅ 提升用户体验

### 长期目标（Phase 4+）

- ✅ 清除所有 🟢 低优先级技术债务
- ✅ 实现完整功能
- ✅ 达到生产级质量

---

## 📊 技术债务趋势

| 阶段 | TODO 数量 | 简化实现数量 | 技术债务评分 |
|------|----------|-------------|-------------|
| Phase 1 完成 | 43 | 15 | 高 |
| Phase 2 完成 | 30 | 12 | 中 |
| Phase 3 完成 | 20 | 18 | 中 |
| **当前** | **20** | **18** | **中** |
| Phase 4 目标 | 5 | 3 | 低 |

---

## 🔍 技术债务检测方法

### 1. 关键词搜索

```bash
# 搜索 TODO 注释
grep -r "TODO" src/

# 搜索简化实现
grep -r "简化\|simplified" src/

# 搜索临时方案
grep -r "For now\|暂时\|临时" src/

# 搜索占位符
grep -r "placeholder\|stub\|mock" src/
```

### 2. 代码审查重点

- [ ] 所有 `// For now` 注释
- [ ] 所有 `// TODO` 注释
- [ ] 所有 `// 简化实现` 注释
- [ ] 所有启发式判断逻辑
- [ ] 所有错误忽略代码

### 3. 测试覆盖检查

- [ ] 简化实现是否有测试？
- [ ] 边界情况是否覆盖？
- [ ] 错误处理是否测试？

---

## 📝 技术债务管理原则

### 1. 零容忍原则

- ❌ 不允许隐藏技术债务
- ❌ 不允许使用"合理的权衡"掩盖问题
- ✅ 必须立即披露所有技术债务
- ✅ 必须让用户做出知情决策

### 2. 优先级原则

- 🔴 高优先级：阻塞后续开发
- 🟡 中优先级：影响功能完整性
- 🟢 低优先级：影响代码质量

### 3. 透明度原则

- ✅ 所有技术债务必须记录
- ✅ 必须评估影响和风险
- ✅ 必须提供修复方案
- ✅ 必须估算修复时间

---

## 🎓 经验教训

### 1. 技术债务的危害

**问题：** Phase 3 中的模板特化简化实现差点成为"隐藏的炸弹"

**教训：**
- 技术债务必须立即披露
- 不能为了快速通过测试而牺牲质量
- 用户反馈至关重要

### 2. 完整实现的价值

**对比：**

| 方面 | 简化实现 | 完整实现 |
|------|---------|----------|
| 开发时间 | 短 | 长 |
| 测试通过率 | 高 | 高 |
| 后续支持 | 差 | 好 |
| 维护成本 | 高 | 低 |

**结论：** 完整实现虽然花费更多时间，但避免了后续的严重问题

### 3. 透明沟通的重要性

**原则：**
- ✅ 立即披露所有问题
- ✅ 提供完整的影响分析
- ✅ 让用户做出知情决策
- ✅ 立即执行用户的选择

---

## 📅 下一步行动

1. **立即行动**（今天）
   - [ ] 审查本清单的完整性
   - [ ] 确认优先级分配
   - [ ] 分配修复任务

2. **本周行动**（Week 1）
   - [ ] 开始修复高优先级技术债务
   - [ ] 每日更新修复进度
   - [ ] 代码审查确保质量

3. **下周行动**（Week 2）
   - [ ] 继续修复高优先级技术债务
   - [ ] 开始中优先级技术债务
   - [ ] 准备 Phase 4 启动

---

**最后更新：** 2026-04-16
**下次审计：** Phase 4 启动前
