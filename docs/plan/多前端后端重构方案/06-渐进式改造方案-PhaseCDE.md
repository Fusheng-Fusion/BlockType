# 三、渐进式改造方案 — Phase C/D/E

> Phase C 后端抽象层、Phase D 管线重构、Phase E 验证迁移

---

## Phase C：后端抽象层 + LLVM 后端适配

**目标**：建立 BackendBase/BackendRegistry，实现 LLVMBackend，实现 IRToLLVMConverter。

### 红线对照

| 红线 | 合规性 | 说明 |
|------|--------|------|
| R1 架构优先 | ✓ | 后端通过抽象接口与前端解耦 |
| R2 自由组合 | ✓ | 后端可独立替换 |
| R3 渐进式 | ✓ | LLVMBackend与现有编译路径并行 |
| R4 功能不退化 | ✓ | 现有功能通过新路径等价实现 |
| R5 接口抽象 | ✓ | BackendBase抽象接口 |
| R6 IR中间层解耦 | ✓ | 后端消费IRModule，不直接接触AST |

### Task 分解

| Task | 内容 | 预估 | 文件变更 | 验收标准 | 风险 |
|------|------|------|---------|---------|------|
| C.1 | BackendBase+BackendRegistry | 1天 | 新增4个文件 | 注册/创建后端正常 | 低 |
| C.1.1 | BackendCapability+LLVM后端能力声明 | 1天 | 新增2个文件 | BackendCapability声明正确、能力检查集成到编译管线 | 低 |
| C.2 | IRToLLVMConverter:类型转换 | 2天 | 新增2个文件 | 所有IRType→llvm::Type转换正确 | 中 |
| C.3 | IRToLLVMConverter:值/指令转换 | 3天 | 新增2个文件 | 所有IR指令→LLVM IR指令转换正确 | 中 |
| C.4 | IRToLLVMConverter:函数/模块转换 | 2天 | 新增2个文件 | IRModule→llvm::Module转换正确 | 中 |
| C.5 | LLVMBackend:优化pipeline | 1天 | 新增2个文件 | 复用现有优化逻辑 | 低 |
| C.6 | LLVMBackend:目标代码生成 | 1天 | 新增2个文件 | 复用现有TargetMachine逻辑 | 低 |
| C.7 | LLVMBackend:调试信息转换 | 2天 | 新增2个文件 | IR调试元数据→DWARF | 中 |
| C.8 | LLVMBackend:VTable/RTTI生成 | 2天 | 新增2个文件 | VTable/RTTI在LLVM后端生成 | 高 |
| C.9 | 集成测试 | 2天 | 新增测试文件 | 端到端编译测试通过 | 中 |

**Phase C 总计**：~28.5天（原16天 + 新增1天C.1.1 + 附加增强11.5人日）

### 附加增强任务（行业前沿补充）

> 来源：09-行业前沿补充优化方案 §3.3

| 任务ID | 任务描述 | 依赖 | 工作量估算 |
|--------|---------|------|-----------|
| C-F1 | PluginManager 基础框架 | Phase C FrontendRegistry | 2人日 |
| C-F2 | CompilerPlugin 基类 + 加载机制 | C-F1 | 2人日 |
| C-F3 | FFIFunctionDecl 基础实现 | Phase C 前端适配层 | 2人日 |
| C-F4 | FFITypeMapper C语言映射 | C-F3 | 1.5人日 |
| C-F5 | 前端模糊测试器 FrontendFuzzer 基础 | Phase C | 1.5人日 |
| C-F6 | SARIF格式诊断输出 | B-F6 | 1人日 |
| C-F7 | FixIt Hint 基础实现 | B-F6 | 1.5人日 |

**Phase C 附加增强工作量**：约 11.5 人日

### 文件变更清单

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Backend/BackendBase.h` |
| 新增 | `include/blocktype/Backend/BackendRegistry.h` |
| 新增 | `include/blocktype/Backend/BackendOptions.h` |
| 新增 | `include/blocktype/Backend/BackendCapability.h` |
| 新增 | `include/blocktype/Backend/IRToLLVMConverter.h` |
| 新增 | `include/blocktype/Backend/LLVMBackend.h` |
| 新增 | `include/blocktype/Backend/ABIInfo.h`（从CodeGen移入） |
| 新增 | `src/Backend/` 下对应所有 .cpp 文件 |
| 新增 | `src/Backend/CMakeLists.txt` |

---

## Phase D：编译管线重构

**目标**：重构 CompilerInstance 为管线编排器，支持 `--frontend`/`--backend` 选项。

### 红线对照

| 红线 | 合规性 | 说明 |
|------|--------|------|
| R1 架构优先 | ✓ | CompilerInstance成为纯粹的编排器 |
| R2 自由组合 | ✓ | 前端/后端通过选项自由组合 |
| R3 渐进式 | ✓ | 渐进替换，旧路径保留到Phase E |
| R4 功能不退化 | ✓ | 默认行为不变（cpp+llvm） |
| R5 接口抽象 | ✓ | 通过FrontendBase*/BackendBase*接口操作 |
| R6 IR中间层解耦 | ✓ | 编排器只传递IRModule |

### Task 分解

| Task | 内容 | 预估 | 文件变更 | 验收标准 | 风险 |
|------|------|------|---------|---------|------|
| D.1 | CompilerInvocation新增FrontendName/BackendName选项 | 1天 | 修改2个文件 | 命令行解析正确 | 低 |
| D.2 | CompilerInstance重构为管线编排器 | 2天 | 修改2个文件 | 新管线编译通过 | 中 |
| D.3 | 保留旧路径作为fallback | 1天 | 修改1个文件 | 无frontend/backend选项时走旧路径 | 低 |
| D.4 | 自动注册机制(CppFrontendRegistration/LLVMBackendRegistration) | 1天 | 新增2个文件 | 前端/后端自动注册正常 | 低 |
| D.5 | 集成测试 | 1天 | 新增测试文件 | 所有编译场景通过 | 中 |

**Phase D 总计**：~21.5天（原6天 + 附加增强15.5人日）

### 附加增强任务（行业前沿补充）

> 来源：09-行业前沿补充优化方案 §3.4

| 任务ID | 任务描述 | 依赖 | 工作量估算 |
|--------|---------|------|-----------|
| D-F1 | InstructionSelector 接口定义 | Phase D BackendBase | 1人日 |
| D-F2 | TargetInstruction 内部表示 | D-F1 | 1人日 |
| D-F3 | RegisterAllocator 接口定义 | Phase D | 1人日 |
| D-F4 | RegAllocFactory + Greedy/Basic实现 | D-F3 | 3人日 |
| D-F5 | DebugInfoEmitter 接口定义 | A-F5 | 0.5人日 |
| D-F6 | DWARF5调试信息发射（LLVM后端） | D-F5, Phase D LLVM后端 | 3人日 |
| D-F7 | 差分测试框架 DifferentialTester | Phase D 多后端 | 2人日 |
| D-F8 | IR模糊测试器 IRFuzzer 基础 | Phase D | 2人日 |
| D-F9 | 后端管线模块化拆分 | D-F1, D-F3 | 2人日 |

**Phase D 附加增强工作量**：约 15.5 人日

### 文件变更清单

| 操作 | 文件路径 |
|------|----------|
| 修改 | `include/blocktype/Frontend/CompilerInstance.h` |
| 修改 | `src/Frontend/CompilerInstance.cpp` |
| 修改 | `include/blocktype/Frontend/CompilerInvocation.h` |
| 修改 | `src/Frontend/CompilerInvocation.cpp` |
| 新增 | `src/Frontend/CppFrontendRegistration.cpp` |
| 新增 | `src/Backend/LLVMBackendRegistration.cpp` |

---

## Phase E：验证和迁移

**目标**：所有现有功能迁移到新架构，移除旧路径依赖，全面验证。

### 红线对照

| 红线 | 合规性 | 说明 |
|------|--------|------|
| R1 架构优先 | ✓ | 最终架构完全符合设计 |
| R2 自由组合 | ✓ | 前后端完全解耦 |
| R3 渐进式 | ✓ | 每个迁移步骤可独立验证 |
| R4 功能不退化 | ✓ | 全量测试回归 |
| R5 接口抽象 | ✓ | 无直接依赖具体实现 |
| R6 IR中间层解耦 | ✓ | 前后端仅通过IR交互 |

### Task 分解

| Task | 内容 | 预估 | 文件变更 | 验收标准 | 风险 |
|------|------|------|---------|---------|------|
| E.1 | 全量测试回归(新管线) | 2天 | 无修改 | 所有现有测试通过 | 中 |
| E.2 | 性能基准测试 | 1天 | 新增基准 | 编译速度/代码质量不低于旧路径 | 中 |
| E.3 | 移除旧路径fallback | 1天 | 修改2-3个文件 | 编译通过，旧路径代码移除 | 低 |
| E.4 | CodeGen模块清理 | 2天 | 重构多个文件 | CodeGen仅被LLVM后端使用 | 中 |
| E.5 | 文档更新 | 1天 | 更新文档 | 架构文档与代码一致 | 低 |

**Phase E 总计**：~22天（原7天 + 附加增强15人日）

### 附加增强任务（行业前沿补充）

> 来源：09-行业前沿补充优化方案 §3.5

| 任务ID | 任务描述 | 依赖 | 工作量估算 |
|--------|---------|------|-----------|
| E-F1 | CompilationCacheManager 完整集成 | B-F8 | 2人日 |
| E-F2 | IR缓存序列化/反序列化 | E-F1 | 2人日 |
| E-F3 | IREquivalenceChecker 语义等价检查 | B-F9 | 3人日 |
| E-F4 | Chrome Trace格式输出 | B-F5 | 1人日 |
| E-F5 | 可重现构建模式实现 | A-F8 | 2人日 |
| E-F6 | IR完整性验证集成 | A-F8 | 1人日 |
| E-F7 | FFICoerce/FFIUnwind 指令实现 | C-F3 | 2人日 |
| E-F8 | CodeView调试信息（Windows后端） | D-F5 | 2人日 |

**Phase E 附加增强工作量**：约 15 人日

### 文件变更清单

| 操作 | 文件路径 |
|------|----------|
| 修改 | 所有现有测试用例（添加新路径测试） |
| 删除 | `src/CodeGen/CodeGenModule.cpp`（旧路径，迁移完成后） |
| 删除 | `src/CodeGen/CodeGenFunction.cpp` |
| 删除 | `src/CodeGen/CodeGenExpr.cpp` |
| 删除 | `src/CodeGen/CodeGenStmt.cpp` |
| 删除 | `src/CodeGen/CodeGenTypes.cpp` |
| 删除 | `src/CodeGen/CodeGenConstant.cpp` |
| 删除 | `src/CodeGen/CGCXX.cpp` |
| 删除 | `src/CodeGen/CGDebugInfo.cpp` |
| 修改 | `CMakeLists.txt`（移除旧 CodeGen 源文件） |
