# Phase 8 详细开发计划：目标平台与优化

> **规划人：** planner | **日期：** 2026-04-23
> **阶段：** Phase 8 — 目标平台与优化
> **目标：** 支持 Linux x86_64 和 macOS ARM64，实现优化 Pass
> **团队模式：** 串行执行（planner → dev-tester → reviewer）

---

## 一、代码库现状分析

### 1.1 已有基础设施

| 组件 | 文件 | 状态 | 说明 |
|------|------|------|------|
| **TargetInfo** | `CodeGen/TargetInfo.h/.cpp` | ✅ 存在 | 基础框架，支持 DataLayout、类型大小/对齐、平台三元组 |
| **Mangler** | `CodeGen/Mangler.h/.cpp` | ✅ 存在 | Itanium ABI 名称修饰完整实现，53 个测试 |
| **CodeGenModule** | `CodeGen/CodeGenModule.h` | ✅ 存在 | 已集成 TargetInfo、Mangler |
| **CodeGenTypes** | `CodeGen/CodeGenTypes.h` | ✅ 存在 | 已有 `ABIArgInfo`、`FunctionABITy`、`needsSRet()`、`shouldUseInReg()` |
| **CompilerInvocation** | `Frontend/CompilerInvocation.h` | ✅ 存在 | 已有 `TargetOptions`、`CodeGenOptions`（OptimizationLevel） |
| **CompilerInstance** | `Frontend/CompilerInstance.cpp` | ✅ 存在 | `runOptimizationPasses()` 和 `generateObjectFile()` 为空壳 |

### 1.2 需要增强的部分

| 组件 | 现状差距 | Phase 8 需要做什么 |
|------|---------|-------------------|
| **TargetInfo** | 仅区分 32/64 位 DataLayout；`isStructReturnInRegister()` 硬编码 ≤16 字节 | 增加平台特化子类，完善类型大小/对齐/ABI 规则 |
| **Mangler** | 缺少 substitution 压缩、高级模板表达式 | 增强 Itanium ABI 兼容性 |
| **ABI 分类** | `needsSRet()` 仅判断 record+大小阈值；`shouldUseInReg()` 返回 false | 实现 System V AMD64 ABI 和 AAPCS64 完整分类 |
| **内建函数** | 仅有 `__builtin_trap` | 实现常用 `__builtin_*` 函数 |
| **调用约定** | 未设置 calling convention 属性 | 设置正确属性（sret/inreg/signext 等） |
| **优化 Pass** | `runOptimizationPasses()` 空壳 | 集成 LLVM PassBuilder |
| **目标代码生成** | `generateObjectFile()` 空壳 | 使用 TargetMachine 生成 .o 文件 |

---

## 二、波次策略

```
Wave 1（1.5 周）: 基础设施层
  8.1.1 TargetInfo 增强 → 8.1.2 ABI 支持

Wave 2（1.5 周）: 平台特定功能
  8.1.3 名称修饰增强 + 8.2.1 内建函数 + 8.2.2 调用约定

Wave 3（1 周）: 优化和代码生成
  8.3.1 基本优化 → 8.3.2 目标特定优化
```

依赖关系图：
```
8.1.1 ──→ 8.1.2 ──→ 8.2.2 ──→ 8.3.2
  │                    ↑          ↑
  ├──→ 8.1.3          │          │
  ├──→ 8.2.1           │          │
  └──→ 8.3.1 ──────────┘─────────┘
```

---

## 三、各 Task 详细开发计划

### Task 8.1.1 — TargetInfo 类增强（7h）

**目标：** 将 TargetInfo 从简单平台区分升级为支持 Linux x86_64 和 macOS ARM64 的完整目标信息

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/CodeGen/TargetInfo.h` | 修改 | 增加虚方法、工厂方法 |
| `src/CodeGen/TargetInfo.cpp` | 修改 | 实现 X86_64TargetInfo、AArch64TargetInfo |
| `src/CodeGen/CodeGenModule.cpp` | 修改 | 使用工厂方法 `TargetInfo::Create()` |
| `tests/unit/CodeGen/TargetInfoTest.cpp` | 新建 | 单元测试 |

#### 开发步骤

1. **重构 TargetInfo 为抽象基类**（2h）
   - 关键方法加 `virtual`：`isStructReturnInRegister()`、`isThisPassedInRegister()`
   - 新增虚方法：`isLinux()`、`isMacOS()`、`isX86_64()`、`isAArch64()`、`getLongWidth()`、`getLongDoubleWidth()`、`getMaxVectorAlign()`
   - 添加 `static std::unique_ptr<TargetInfo> Create(llvm::StringRef Triple)` 工厂方法

2. **实现 X86_64TargetInfo**（2h）
   - DataLayout: `"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"`
   - long = 64bit, long double = 128bit (x87 extended padded)
   - `isStructReturnInRegister()`: System V AMD64 规则（≤16 字节且可分类为 INTEGER/SSE）

3. **实现 AArch64TargetInfo**（1.5h）
   - DataLayout: `"e-m:o-i64:64-i128:128-n32:64-S128"`
   - long = 64bit, long double = 64bit (IEEE double on Darwin)
   - `isStructReturnInRegister()`: AAPCS64 规则（≤16 字节非 MEMORY 类）

4. **更新 CodeGenModule**（0.5h）
   - `Target = TargetInfo::Create(TargetTriple)` 替代 `make_unique<TargetInfo>(TargetTriple)`

5. **编写测试**（1h）
   - X86_64/AArch64 平台信息、工厂方法分发、long double 大小区别

---

### Task 8.1.2 — ABI 支持（16h）

**目标：** 实现完整的 ABI 分类逻辑，替代简化实现

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/CodeGen/ABIInfo.h` | 新建 | ABI 分类器接口 |
| `src/CodeGen/ABIInfo.cpp` | 新建 | X86_64ABIInfo + AArch64ABIInfo |
| `include/blocktype/CodeGen/CodeGenTypes.h` | 修改 | 扩展 ABIArgKind |
| `src/CodeGen/CodeGenTypes.cpp` | 修改 | 使用 ABIInfo 替代简化逻辑 |
| `src/CodeGen/CMakeLists.txt` | 修改 | 添加 ABIInfo.cpp |
| `tests/unit/CodeGen/ABIInfoTest.cpp` | 新建 | ABI 分类器测试 |

#### 开发步骤

1. **创建 ABIInfo 基类**（1.5h）
   ```cpp
   class ABIInfo {
   public:
     virtual ~ABIInfo() = default;
     virtual ABIArgInfo classifyReturnType(QualType RetTy) const = 0;
     virtual ABIArgInfo classifyArgumentType(QualType ParamTy) const = 0;
   };
   ```

2. **实现 X86_64ABIInfo**（5h）— 最复杂
   - System V AMD64 ABI 分类算法：INTEGER / SSE / SSEUP / X87 / X87UP / MEMORY
   - 结构体递归分类：遍历字段，合并分类结果（merge algorithm）
   - 返回值：≤16 字节且非 MEMORY → 寄存器返回，否则 sret
   - `__int128` 通过两个寄存器传递

3. **实现 AArch64ABIInfo**（4h）
   - AAPCS64 分类：INTEGER / VECTOR / FP / MEMORY
   - HFA/HVA 检测（Homogeneous Floating-point/Vector Aggregate）
   - 结构体 ≤16 字节按字段分类，>16 字节 MEMORY

4. **扩展 ABIArgKind**（1h）
   - 增加 `Expand`（结构体按字段展开传递）和 `Coerce`（类型强制转换）

5. **集成到 CodeGenTypes**（2h）
   - `GetFunctionABI()` 使用 `ABIInfo` 替代简化逻辑
   - `needsSRet()` / `shouldUseInReg()` 委托给 ABIInfo

6. **编写测试**（2.5h）

---

### Task 8.1.3 — 名称修饰增强（9h）

**目标：** 添加 substitution 压缩，增强 Itanium ABI 兼容性

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/CodeGen/Mangler.h` | 修改 | 添加 substitution 表 |
| `src/CodeGen/Mangler.cpp` | 修改 | 实现 substitution 压缩 |
| `tests/unit/CodeGen/ManglerTest.cpp` | 修改 | 添加 substitution 测试 |

#### 开发步骤

1. **添加 substitution 表**（4h）
   - `llvm::SmallVector<std::string, 16> Substitutions`
   - 序列号编码：`S_`（第0个）、`S0_`（第1个）...
   - 标准实体预定义：`St`=std::, `Sa`=std::allocator 等

2. **增强模板表达式**（2h）
   - template template parameter
   - nested template name

3. **跨平台验证**（1.5h）
   - 使用 `c++filt` 验证 mangled name 可正确 demangle

4. **添加测试**（1.5h）

---

### Task 8.2.1 — 内建函数（11h）

**目标：** 实现 GCC/Clang 兼容的常用内建函数

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/Basic/Builtins.def` | 新建 | 内建函数定义表 |
| `include/blocktype/Basic/Builtins.h` | 新建 | 内建函数查询接口 |
| `src/Basic/Builtins.cpp` | 新建 | 内建函数实现 |
| `src/Basic/CMakeLists.txt` | 修改 | 添加 Builtins.cpp |
| `src/Sema/SemaExpr.cpp` | 修改 | 识别内建函数调用 |
| `src/CodeGen/CodeGenExpr.cpp` | 修改 | 生成内建函数 LLVM IR |
| `tests/CodeGen/builtin_functions.cpp` | 新建 | 端到端测试 |

#### 开发步骤

1. **创建 Builtins.def 定义表**（2h）
   - `BUILTIN(__builtin_abs, "i.", "nc")` 等定义
   - 通用内建：abs, strlen, expect, unreachable, ctz, clz, popcount, bswap*

2. **Sema 中识别内建函数**（2.5h）
   - `__builtin_` 前缀查找 Builtins.def
   - 创建隐式 FunctionDecl

3. **CodeGen 中生成内建调用**（3h）
   - 映射到 LLVM Intrinsic：abs→llvm.abs, ctz→llvm.cttz, clz→llvm.ctlz 等
   - 添加 `EmitBuiltinCall()` 方法

4. **平台特定内建框架**（1.5h）
   - x86 `__builtin_ia32_*` 和 ARM `__builtin_arm_*` 仅定义框架，标记 TODO

5. **编写测试**（2h）

---

### Task 8.2.2 — 调用约定（8h）

**目标：** 在 LLVM IR 层面正确设置调用约定属性

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/CodeGen/CodeGenModule.cpp` | 修改 | 设置函数 calling convention |
| `src/CodeGen/CodeGenFunction.cpp` | 修改 | 设置调用属性 |
| `src/CodeGen/CodeGenExpr.cpp` | 修改 | 间接调用 calling convention |
| `tests/CodeGen/calling_convention.cpp` | 新建 | 测试 |

#### 开发步骤

1. **设置函数 calling convention**（1.5h）
   - `Fn->setCallingConv(llvm::CallingConv::C)` — x86_64/AArch64 默认

2. **设置参数属性**（2h）
   - SRet: `sret` + `align` 属性
   - InReg: `inreg` 属性
   - ByVal: `byval` + `align` 属性

3. **设置返回值属性**（1.5h）
   - sret 返回值：void 返回 + 隐式首参数
   - signext/zeroext：i1/i8/i16 等小类型

4. **间接调用处理**（1.5h）
   - 虚函数调用、函数指针调用设置 calling convention

5. **编写测试**（1.5h）

---

### Task 8.3.1 — 基本优化（8h）

**目标：** 集成 LLVM PassBuilder，实现 -O0/-O1/-O2/-O3

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/Frontend/CompilerInstance.cpp` | 修改 | 实现 `runOptimizationPasses()` 和 `generateObjectFile()` |
| `tests/CodeGen/optimization.cpp` | 新建 | 优化测试 |

#### 开发步骤

1. **实现 runOptimizationPasses()**（3h）
   - 使用 LLVM PassBuilder + PassBuilder::buildPerModuleDefaultPipeline()
   - O0: `buildO0DefaultPipeline()`
   - O1/O2/O3: `buildPerModuleDefaultPipeline(O1/O2/O3)`
   - 注册所有 Analysis Manager

2. **实现 generateObjectFile()**（3h）
   - `InitializeAllTargets()` / `InitializeAllTargetMCs()` / `InitializeAllAsmPrinters()`
   - `TargetRegistry::lookupTarget()` 查找目标
   - `Target::createTargetMachine()` 创建 TargetMachine
   - `TargetMachine::addPassesToEmitFile()` 生成 .o 文件

3. **验证优化选项传递**（0.5h）

4. **编写测试**（1.5h）

---

### Task 8.3.2 — 目标特定优化（7.5h）

**目标：** 实现目标平台特定的优化配置

#### 文件变更

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/Frontend/CompilerInstance.cpp` | 修改 | 目标特定优化配置 |
| `include/blocktype/Frontend/CompilerInvocation.h` | 修改 | 添加目标特定选项 |
| `tests/CodeGen/target_optimization.cpp` | 新建 | 测试 |

#### 开发步骤

1. **x86_64 特定优化**（2h）
   - SSE/AVX 向量化配置
   - 默认 `-mcpu=x86-64-v2`
   - `TargetOptions::FloatABIType = FloatABI::Hard`

2. **AArch64 特定优化**（2h）
   - NEON 向量化配置
   - 默认 `-mcpu=apple-m1`
   - AAPCS64 硬浮点

3. **TargetMachine 优化配置**（2h）
   - CodeGenOptLevel 设置
   - PIC/PIE 模式
   - 调试信息选项

4. **编写测试**（1.5h）

---

## 四、关键风险与缓解措施

### 风险 1：System V AMD64 ABI 分类算法复杂度（高）

**描述：** x86_64 结构体分类算法（merging classification words）是 Phase 8 最复杂部分，涉及递归分类和 8 字节对齐的九宫格合并

**缓解：**
- 参考 Clang 的 `X86_64ABIInfo::classify()` 实现（约 500 行）
- 先实现简化版（仅处理 INTEGER/SSE/MEMORY 三类），再逐步完善
- 编写大量对比测试，用 GCC/Clang 生成的符号作为参照

### 风险 2：LLVM PassBuilder API 版本差异（中）

**描述：** BlockType 依赖 LLVM 17+，不同 LLVM 版本的 PassBuilder API 可能有差异

**缓解：**
- 编译时检查 LLVM 版本宏
- 优先使用稳定 API（`buildPerModuleDefaultPipeline` 等）
- 避免使用 `Experimental` 标记的 API

### 风险 3：目标代码生成首次集成可能遇到链接问题（中）

**描述：** `generateObjectFile()` 需要正确初始化 LLVM TargetRegistry，可能遗漏初始化步骤

**缓解：**
- 确保 `InitializeAllTargets()` 等初始化函数在 main() 中调用
- 参考 LLVM kaleidoscope 教程的实现模式
- 先在单一平台（macOS ARM64）验证，再扩展到 Linux x86_64

### 风险 4：内建函数类型签名与平台 ABI 不匹配（低）

**描述：** 内建函数的参数/返回类型在不同平台可能有差异（如 `size_t` 在 32/64 位）

**缓解：**
- 使用 TargetInfo 查询类型大小，不硬编码
- 参考 Clang Builtins.def 的签名定义

---

## 五、工作量汇总

| Task | 预估时间 | Wave |
|------|---------|------|
| 8.1.1 TargetInfo 增强 | 7h | Wave 1 |
| 8.1.2 ABI 支持 | 16h | Wave 1 |
| 8.1.3 名称修饰增强 | 9h | Wave 2 |
| 8.2.1 内建函数 | 11h | Wave 2 |
| 8.2.2 调用约定 | 8h | Wave 2 |
| 8.3.1 基本优化 | 8h | Wave 3 |
| 8.3.2 目标特定优化 | 7.5h | Wave 3 |
| **合计** | **66.5h** | — |

### Wave 时间线

| Wave | Tasks | 预估时间 | 日历时间 |
|------|-------|---------|---------|
| Wave 1 | 8.1.1 + 8.1.2 | 23h | 1.5 周 |
| Wave 2 | 8.1.3 + 8.2.1 + 8.2.2 | 28h | 1.5 周 |
| Wave 3 | 8.3.1 + 8.3.2 | 15.5h | 1 周 |
| **总计** | — | **66.5h** | **4 周** |

---

## 六、验收检查清单

```
[ ] TargetInfo 工厂方法正确分发 X86_64/AArch64
[ ] X86_64 DataLayout 和类型大小正确
[ ] AArch64 DataLayout 和类型大小正确
[ ] long double 大小：Linux 16 字节 vs macOS 8 字节
[ ] X86_64ABIInfo 分类算法通过测试
[ ] AArch64ABIInfo 分类算法通过测试
[ ] sret/inreg/signext 属性正确设置
[ ] Mangler substitution 压缩工作
[ ] __builtin_abs/strlen/expect/ctz/clz/popcount 工作
[ ] -O0/-O1/-O2/-O3 优化级别工作
[ ] generateObjectFile() 生成有效 .o 文件
[ ] macOS ARM64 目标代码正确
[ ] Linux x86_64 目标代码正确（交叉编译）
[ ] 所有现有测试通过（回归测试）
```

---

*Phase 8 完成标志：支持 Linux x86_64 和 macOS ARM64 目标平台；优化 Pass 正确工作；能生成可执行文件。*
