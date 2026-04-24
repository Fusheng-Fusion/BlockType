# 三、渐进式改造方案 — Phase A/B

> 融合方案1的细粒度Task拆分+每Phase红线对照，和方案0的Phase B/C并行策略

---

## 改造阶段总览

```
Phase A ──→ Phase B ──→ Phase C ──→ Phase D ──→ Phase E ──→ Phase F ──→ Phase G ──→ Phase H
IR基础设施    前端抽象层   后端抽象层    管线重构      验证迁移    高级特性期   增量编译期   AI辅助与极致优化期
```

每个 Phase 完成后必须：编译通过 + 全部现有测试通过 + 新增 IR 层测试通过。

**Phase B/C 并行机会**（取自方案0）：
- Phase B 的 Task B.1-B.2（前端接口+基础转换）与 Phase C 的 Task C.1（后端接口）可并行
- Phase B 的 Task B.4-B.6（表达式/语句/C++ 特有）与 Phase C 的 Task C.2（IR→LLVM 转换）可部分并行

---

## Phase A：IR 层基础设施

**目标**：建立独立的 IR 库，包含类型系统、值系统、构建器、验证器、序列化。

### 红线对照

| 红线 | 合规性 | 说明 |
|------|--------|------|
| R1 架构优先 | ✓ | IR层独立于LLVM和AST，架构正确 |
| R2 自由组合 | ✓ | IR层是前后端自由组合的基础 |
| R3 渐进式 | ✓ | 纯新增代码，不影响现有功能 |
| R4 功能不退化 | ✓ | 现有功能零影响 |
| R5 接口抽象 | ✓ | IR层全部通过抽象接口设计 |
| R6 IR中间层解耦 | ✓ | IR层实现前后端解耦的核心机制 |

### Task 分解

| Task | 内容 | 预估 | 文件变更 | 验收标准 | 风险 |
|------|------|------|---------|---------|------|
| A.1 | IRType体系+IRTypeContext | 2天 | 新增6个文件 | 所有类型可创建、缓存、比较 | 低 |
| A.1.1 | IRContext+BumpPtrAllocator内存管理 | 1.5天 | 新增2个文件 | IRContext可分配IR节点、清理回调正确、性能优于new | 低 |
| A.1.2 | IRThreadingMode枚举+seal接口 | 0.5天 | 修改IRContext | 枚举定义正确、seal接口预留 | 低 |
| A.2 | TargetLayout(独立于LLVM DataLayout) | 1天 | 新增2个文件 | x86_64/aarch64布局正确 | 低 |
| A.3 | IRValue+IRConstant+Use/User体系 | 2天 | 新增4个文件 | 常量可创建、use-def chain正确 | 低 |
| A.3.1 | IRFormatVersion+IRFileHeader | 1天 | 新增2个文件 | 版本兼容性检查正确、二进制头序列化/反序列化 | 低 |
| A.4 | IRModule/IRFunction/IRBasicBlock/IRFunctionDecl | 2天 | 新增4个文件 | 模块结构可构建 | 低 |
| A.5 | IRBuilder(含常量工厂) | 2天 | 新增2个文件 | 所有指令可创建 | 低 |
| A.6 | IRVerifier | 1天 | 新增2个文件 | 验证规则全部实现 | 低 |
| A.7 | IRSerializer(文本+二进制格式) | 1天 | 新增2个文件 | IR可序列化/反序列化 | 低 |
| A.8 | CMake集成+单元测试 | 1天 | CMakeLists.txt | libblocktype-ir编译通过，测试通过 | 低 |

**Phase A 总计**：~20天（原12天 + 新增2.5天 + 附加增强5.5人日）

### 附加增强任务（行业前沿补充）

> 来源：09-行业前沿补充优化方案 §3.1

| 任务ID | 任务描述 | 依赖 | 工作量估算 |
|--------|---------|------|-----------|
| A-F1 | IRInstruction/IRType 添加 DialectID 字段（默认 Core） | 无 | 0.5人日 |
| A-F2 | DialectCapability 扩展 BackendCapability | A-F1 | 0.5人日 |
| A-F3 | TelemetryCollector 基础框架 + PhaseGuard RAII | 无 | 1人日 |
| A-F4 | IRInstruction 添加 Optional DbgInfo 字段 | 无 | 0.5人日 |
| A-F5 | IRDebugMetadata 基础类型定义 | A-F4 | 1人日 |
| A-F6 | StructuredDiagnostic 基础结构定义 | 无 | 0.5人日 |
| A-F7 | CacheKey/CacheEntry 基础类型定义 | 无 | 0.5人日 |
| A-F8 | IRIntegrityChecksum 基础实现 | 无 | 1人日 |

**Phase A 附加增强工作量**：约 5.5 人日

### 文件变更清单

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/IRType.h` |
| 新增 | `include/blocktype/IR/IRTypeContext.h` |
| 新增 | `include/blocktype/IR/IRContext.h` |
| 新增 | `include/blocktype/IR/TargetLayout.h` |
| 新增 | `include/blocktype/IR/IRValue.h` |
| 新增 | `include/blocktype/IR/IRInstruction.h` |
| 新增 | `include/blocktype/IR/IRConstant.h` |
| 新增 | `include/blocktype/IR/IRFormatVersion.h` |
| 新增 | `include/blocktype/IR/IRModule.h` |
| 新增 | `include/blocktype/IR/IRFunction.h` |
| 新增 | `include/blocktype/IR/IRBasicBlock.h` |
| 新增 | `include/blocktype/IR/IRBuilder.h` |
| 新增 | `include/blocktype/IR/IRVerifier.h` |
| 新增 | `include/blocktype/IR/IRSerializer.h` |
| 新增 | `include/blocktype/IR/IRDebugInfo.h` |
| 新增 | `include/blocktype/IR/IRMetadata.h` |
| 新增 | `include/blocktype/IR/IRConsumer.h` |
| 新增 | `include/blocktype/IR/IRPass.h` |
| 新增 | `include/blocktype/IR/BackendCapability.h` |
| 新增 | `src/IR/` 下对应所有 .cpp 文件 |
| 新增 | `src/IR/CMakeLists.txt` |
| 修改 | `CMakeLists.txt`（添加 IR 子目录） |

---

## Phase B：前端抽象层 + C++ 前端适配

**目标**：建立 FrontendBase/FrontendRegistry，实现 CppFrontend，实现 ASTToIRConverter。

### 红线对照

| 红线 | 合规性 | 说明 |
|------|--------|------|
| R1 架构优先 | ✓ | 前端通过抽象接口与后端解耦 |
| R2 自由组合 | ✓ | 前端可独立替换 |
| R3 渐进式 | ✓ | CppFrontend与现有CodeGenModule并行存在，三阶段共存策略 |
| R4 功能不退化 | ✓ | 现有编译路径不变，新路径独立验证 |
| R5 接口抽象 | ✓ | FrontendBase抽象接口 |
| R6 IR中间层解耦 | ✓ | 前端产出IRModule，后端消费IRModule |

### Task 分解

| Task | 内容 | 预估 | 文件变更 | 验收标准 | 风险 |
|------|------|------|---------|---------|------|
| B.1 | FrontendBase+FrontendRegistry(含autoSelect) | 1天 | 新增4个文件 | 注册/创建/自动选择前端正常 | 低 |
| B.2 | IRMangler(独立于CodeGenModule，依赖TargetLayout) | 2天 | 新增2个文件 | 名称修饰结果与现有Mangler一致 | 中 |
| B.2.1 | IRConversionResult+错误占位策略 | 1.5天 | 新增2个文件 | IRConversionResult正确、错误占位策略工作 | 低 |
| B.3 | IRTypeMapper: QualType→IRType转换 | 2天 | 新增2个文件 | 所有QualType→IRType转换正确 | 中 |
| B.3.1 | DeadFunctionEliminationPass | 1天 | 新增2个文件 | 死函数正确消除、ExternalLinkage函数保留 | 低 |
| B.3.2 | ConstantFoldingPass | 1.5天 | 新增2个文件 | 常量表达式正确折叠 | 中 |
| B.3.3 | TypeCanonicalizationPass | 1天 | 新增2个文件 | OpaqueType消除、结构体偏移计算 | 低 |
| B.3.4 | createDefaultIRPipeline() | 0.5天 | 修改IRPass相关 | Pass管线编排正确 | 低 |
| B.4 | ASTToIRConverter:表达式(基础) | 3天 | 新增2个文件 | BinaryOp/UnaryOp/CallExpr/MemberExpr/DeclRef/Cast转换正确 | 中 |
| B.4.1 | 契约验证函数+FrontendIRContractTest | 2天 | 新增2个文件 | 6个契约验证函数实现、GTest测试通过 | 中 |
| B.5 | ASTToIRConverter:表达式(C++特有) | 3天 | 新增2个文件 | CXXConstruct/New/Delete/This/VirtualCall转换正确 | 高 |
| B.6 | ASTToIRConverter:语句 | 2天 | 新增2个文件 | If/For/While/Return/Switch转换正确 | 中 |
| B.7 | ASTToIRConverter:C++类布局 | 2天 | 新增2个文件 | 类布局计算与现有CGCXX一致 | 高 |
| B.8 | ASTToIRConverter:构造/析构函数 | 3天 | 新增2个文件 | 构造/析构IR生成正确 | 高 |
| B.9 | CppFrontend实现 | 1天 | 新增2个文件 | CppFrontend可编译源文件到IR | 中 |
| B.10 | 集成测试 | 2天 | 新增测试文件 | 现有测试全部通过+IR输出验证 | 中 |

**Phase B 总计**：~42天（原21天 + 新增3天B.2.1 + 新增4天B.3.1-B.3.4 + 新增2天B.4.1 - 3天原B.3/B.4编号偏移 ≈ 24天 + 附加增强18人日）

### 附加增强任务（行业前沿补充）

> 来源：09-行业前沿补充优化方案 §3.2

| 任务ID | 任务描述 | 依赖 | 工作量估算 |
|--------|---------|------|-----------|
| B-F1 | DialectLoweringPass：bt_cpp → bt_core 降级 | A-F1, Phase B IR Pass框架 | 3人日 |
| B-F2 | DialectLoweringPass：invoke→call+landingpad | B-F1 | 2人日 |
| B-F3 | DialectLoweringPass：dynamic_cast→函数调用 | B-F1 | 1人日 |
| B-F4 | DialectLoweringPass：vtable→全局常量+间接调用 | B-F1 | 2人日 |
| B-F5 | TelemetryCollector 与 CompilerInstance 集成 | A-F3 | 1人日 |
| B-F6 | StructuredDiagEmitter 基础实现（文本+JSON） | A-F6 | 2人日 |
| B-F7 | DiagnosticGroupManager 基础实现 | A-F6 | 1人日 |
| B-F8 | LocalDiskCache 基础实现 | A-F7 | 2人日 |
| B-F9 | PassInvariantChecker 基础实现（SSA/类型不变量） | Phase B IR Pass框架 | 2人日 |
| B-F10 | IR调试信息：前端→IR调试元数据传递 | A-F4, A-F5 | 2人日 |

**Phase B 附加增强工作量**：约 18 人日

### CGCXX 拆分策略（在 B.7-B.8 期间执行）

1. 先拆分 CGCXX.cpp 为 7 个子文件（物理拆分，逻辑不变）
2. 然后逐步将各子文件的逻辑迁移到 ASTToIRConverter
3. VTable/RTTI 逻辑留在 LLVM 后端（Itanium ABI 特定）

### 文件变更清单

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Frontend/FrontendBase.h` |
| 新增 | `include/blocktype/Frontend/FrontendRegistry.h` |
| 新增 | `include/blocktype/Frontend/FrontendOptions.h` |
| 新增 | `include/blocktype/Frontend/ASTToIRConverter.h` |
| 新增 | `include/blocktype/Frontend/IREmitExpr.h` |
| 新增 | `include/blocktype/Frontend/IREmitStmt.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXX.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXLayout.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXCtor.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXDtor.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXVTable.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXRTTI.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXInherit.h` |
| 新增 | `include/blocktype/Frontend/IREmitCXXThunk.h` |
| 新增 | `include/blocktype/Frontend/IRConstantEmitter.h` |
| 新增 | `include/blocktype/Frontend/IRTypeMapper.h` |
| 新增 | `include/blocktype/Frontend/IRDebugInfo.h` |
| 新增 | `src/Frontend/` 下对应所有 .cpp 文件 |
| 修改 | `include/blocktype/CodeGen/Mangler.h`（去掉 CodeGenModule 依赖） |
| 修改 | `src/CodeGen/Mangler.cpp`（改为依赖 ASTContext+TargetLayout） |
| 移动 | `include/blocktype/CodeGen/TargetInfo.h` → `include/blocktype/Target/TargetInfo.h` |
| 移动 | `include/blocktype/CodeGen/ABIInfo.h` → `include/blocktype/Backend/ABIInfo.h` |
