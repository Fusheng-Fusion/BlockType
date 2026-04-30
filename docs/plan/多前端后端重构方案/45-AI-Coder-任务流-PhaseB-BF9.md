# AI Coder 可执行任务流 — Phase B 增强任务 B-F9：PassInvariantChecker 基础实现（SSA/类型不变量）

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype::ir` 中
5. **头文件路径**：`include/blocktype/IR/`，源文件路径：`src/IR/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F9 — PassInvariantChecker 基础实现（SSA/类型不变量）"
   git push origin HEAD
   ```

---

## Task B-F9：PassInvariantChecker 基础实现（SSA/类型不变量）

### 依赖验证

✅ **Phase B IR Pass 框架**：已存在于 `include/blocktype/IR/IRPass.h`
✅ **IRVerifier**：已存在于 `include/blocktype/IR/IRVerifier.h`

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/IR/PassInvariantChecker.h` |
| 新增 | `src/IR/PassInvariantChecker.cpp` |

---

## 必须实现的类型定义

### 1. InvariantKind 枚举

```cpp
// 在 PassInvariantChecker.h 中定义

namespace blocktype {
namespace ir {

/// 不变量类型枚举
enum class InvariantKind : uint8_t {
  // SSA 不变量
  SSA_SingleDefinition    = 0,  // 每个 Value 唯一定义
  SSA_Dominance           = 1,  // 定义支配所有使用
  SSA_PhiNodeConsistency  = 2,  // Phi 节点前驱数量与 BB 前驱数量一致
  
  // 类型不变量
  Type_Completeness       = 10, // 无 OpaqueType 残留
  Type_Consistency        = 11, // 操作数类型与指令要求一致
  Type_FunctionSignature  = 12, // 函数调用参数数量和类型匹配
  
  // 控制流不变量
  CF_SingleTerminator     = 20, // 每个 BB 恰好一个终结指令
  CF_EntryBlock           = 21, // 函数有入口 BB
  CF_Reachability         = 22, // 所有 BB 可达（从入口）
  
  // 内存不变量
  Mem_DefUseChain         = 30, // Use-Def 链双向正确
  Mem_AllocaOwner         = 31, // Alloca 指令在函数入口 BB
  
  // 数据不变量
  Data_NoUndef            = 40, // 无 Undef 值（可选）
  Data_ConstantFoldable   = 41, // 常量表达式已折叠（可选）
};

/// 不变量检查结果
struct InvariantViolation {
  InvariantKind Kind;
  std::string Description;
  const IRValue* Value = nullptr;
  const IRInstruction* Inst = nullptr;
  const IRBasicBlock* BB = nullptr;
  const IRFunction* Fn = nullptr;
};

} // namespace ir
} // namespace blocktype
```

### 2. PassInvariantChecker 类

```cpp
// 在 PassInvariantChecker.h 中定义

namespace blocktype {
namespace ir {

/// Pass 不变量检查器
/// 在每个 Pass 运行前后检查 IR 不变量是否保持
class PassInvariantChecker : public Pass {
  SmallVector<InvariantViolation, 16> Violations;
  bool CheckBefore;
  bool CheckAfter;
  bool FailFast;  // 发现违规立即中止
  
public:
  explicit PassInvariantChecker(bool Before = true, bool After = true, bool FailFast = false)
    : CheckBefore(Before), CheckAfter(After), FailFast(FailFast) {}
  
  StringRef getName() const override { return "invariant-checker"; }
  
  bool run(IRModule& M) override;
  
  // === 不变量检查接口 ===
  
  /// 检查所有不变量
  bool checkAllInvariants(const IRModule& M);
  
  /// 检查 SSA 不变量
  bool checkSSAInvariants(const IRModule& M);
  
  /// 检查类型不变量
  bool checkTypeInvariants(const IRModule& M);
  
  /// 检查控制流不变量
  bool checkControlFlowInvariants(const IRModule& M);
  
  /// 检查内存不变量
  bool checkMemoryInvariants(const IRModule& M);
  
  // === 单个不变量检查 ===
  
  bool checkSingleDefinition(const IRFunction& Fn);
  bool checkDominance(const IRFunction& Fn);
  bool checkPhiNodeConsistency(const IRFunction& Fn);
  bool checkTypeCompleteness(const IRModule& M);
  bool checkTypeConsistency(const IRModule& M);
  bool checkFunctionSignature(const IRModule& M);
  bool checkSingleTerminator(const IRFunction& Fn);
  bool checkEntryBlock(const IRFunction& Fn);
  bool checkReachability(const IRFunction& Fn);
  bool checkDefUseChain(const IRModule& M);
  bool checkAllocaOwner(const IRFunction& Fn);
  
  // === 结果查询 ===
  
  const SmallVector<InvariantViolation, 16>& getViolations() const { return Violations; }
  
  bool hasViolations() const { return !Violations.empty(); }
  
  void clearViolations() { Violations.clear(); }
  
  /// 输出违规报告
  void printViolations(raw_ostream& OS) const;
  
private:
  void reportViolation(InvariantKind Kind, StringRef Desc,
                       const IRValue* V = nullptr,
                       const IRInstruction* I = nullptr,
                       const IRBasicBlock* B = nullptr,
                       const IRFunction* F = nullptr);
};

} // namespace ir
} // namespace blocktype
```

---

## 实现细节

### checkSingleDefinition 实现

```cpp
bool PassInvariantChecker::checkSingleDefinition(const IRFunction& Fn) {
  DenseMap<const IRValue*, const IRInstruction*> Definitions;
  
  for (const auto& BB : Fn.getBasicBlocks()) {
    for (const auto& I : BB->getInstList()) {
      if (I->hasResult()) {
        const IRValue* Result = I;
        
        auto It = Definitions.find(Result);
        if (It != Definitions.end()) {
          // 违规：Value 被多次定义
          reportViolation(InvariantKind::SSA_SingleDefinition,
                          "Value defined multiple times",
                          Result, I.get(), BB.get(), &Fn);
          if (FailFast) return false;
        }
        
        Definitions[Result] = I.get();
      }
    }
  }
  
  return true;
}
```

### checkTypeCompleteness 实现

```cpp
bool PassInvariantChecker::checkTypeCompleteness(const IRModule& M) {
  bool OK = true;
  
  // 检查所有函数类型
  for (const auto& Fn : M.getFunctions()) {
    if (checkTypeComplete(Fn->getFunctionType(), Fn.get())) {
      // 违规：函数类型含 OpaqueType
      if (FailFast) return false;
      OK = false;
    }
  }
  
  // 检查所有全局变量类型
  for (const auto& GV : M.getGlobals()) {
    if (checkTypeComplete(GV->getType(), nullptr, nullptr, nullptr, nullptr, GV.get())) {
      if (FailFast) return false;
      OK = false;
    }
  }
  
  return OK;
}

bool PassInvariantChecker::checkTypeComplete(const IRType* T, const IRFunction* Fn) {
  if (T->isOpaque()) {
    reportViolation(InvariantKind::Type_Completeness,
                    "OpaqueType found in function signature",
                    nullptr, nullptr, nullptr, Fn);
    return true;
  }
  
  // 递归检查子类型
  if (T->isPointer()) {
    return checkTypeComplete(cast<IRPointerType>(T)->getPointeeType(), Fn);
  }
  
  if (T->isArray()) {
    return checkTypeComplete(cast<IRArrayType>(T)->getElementType(), Fn);
  }
  
  if (T->isFunction()) {
    auto* FT = cast<IRFunctionType>(T);
    if (checkTypeComplete(FT->getReturnType(), Fn)) return true;
    for (unsigned i = 0; i < FT->getNumParams(); ++i) {
      if (checkTypeComplete(FT->getParamType(i), Fn)) return true;
    }
  }
  
  if (T->isStruct()) {
    auto* ST = cast<IRStructType>(T);
    for (unsigned i = 0; i < ST->getNumFields(); ++i) {
      if (checkTypeComplete(ST->getFieldType(i), Fn)) return true;
    }
  }
  
  return false;
}
```

### checkSingleTerminator 实现

```cpp
bool PassInvariantChecker::checkSingleTerminator(const IRFunction& Fn) {
  bool OK = true;
  
  for (const auto& BB : Fn.getBasicBlocks()) {
    unsigned TerminatorCount = 0;
    const IRInstruction* FirstTerminator = nullptr;
    
    for (const auto& I : BB->getInstList()) {
      if (I->isTerminator()) {
        ++TerminatorCount;
        if (TerminatorCount == 1) {
          FirstTerminator = I.get();
        } else {
          // 违规：多个终结指令
          reportViolation(InvariantKind::CF_SingleTerminator,
                          "BasicBlock has multiple terminators",
                          nullptr, I.get(), BB.get(), &Fn);
          if (FailFast) return false;
          OK = false;
        }
      }
    }
    
    if (TerminatorCount == 0) {
      // 违规：无终结指令
      reportViolation(InvariantKind::CF_SingleTerminator,
                      "BasicBlock has no terminator",
                      nullptr, nullptr, BB.get(), &Fn);
      if (FailFast) return false;
      OK = false;
    }
  }
  
  return OK;
}
```

### checkDefUseChain 实现

```cpp
bool PassInvariantChecker::checkDefUseChain(const IRModule& M) {
  bool OK = true;
  
  for (const auto& Fn : M.getFunctions()) {
    for (const auto& BB : Fn->getBasicBlocks()) {
      for (const auto& I : BB->getInstList()) {
        // 检查每个操作数的 Use-Def 链
        for (unsigned i = 0; i < I->getNumOperands(); ++i) {
          const IRValue* Op = I->getOperand(i);
          
          // 验证 Op 的 Use 列表中包含 I
          bool Found = false;
          for (const auto& Use : Op->uses()) {
            if (Use.getUser() == I.get() && Use.getOperandNo() == i) {
              Found = true;
              break;
            }
          }
          
          if (!Found) {
            // 违规：Use-Def 链不一致
            reportViolation(InvariantKind::Mem_DefUseChain,
                            "Use-Def chain broken: operand not in use list",
                            Op, I.get(), BB.get(), Fn.get());
            if (FailFast) return false;
            OK = false;
          }
        }
      }
    }
  }
  
  return OK;
}
```

---

## 与 PassManager 集成

```cpp
// 在 PassManager 中添加不变量检查

class PassManager {
  SmallVector<unique_ptr<Pass>, 16> Passes;
  bool EnableInvariantChecking;
  unique_ptr<PassInvariantChecker> InvariantChecker;
  
public:
  explicit PassManager(bool CheckInvariants = false)
    : EnableInvariantChecking(CheckInvariants) {
    if (EnableInvariantChecking) {
      InvariantChecker = std::make_unique<PassInvariantChecker>(true, true, true);
    }
  }
  
  void addPass(unique_ptr<Pass> P) {
    Passes.push_back(std::move(P));
  }
  
  bool run(IRModule& M) {
    for (auto& P : Passes) {
      // 运行前检查
      if (EnableInvariantChecking) {
        if (!InvariantChecker->checkAllInvariants(M)) {
          return false;  // 不变量违规，中止
        }
      }
      
      // 运行 Pass
      bool Changed = P->run(M);
      
      // 运行后检查
      if (EnableInvariantChecking && Changed) {
        if (!InvariantChecker->checkAllInvariants(M)) {
          // Pass 破坏了不变量
          errs() << "Pass '" << P->getName() << "' violated invariants:\n";
          InvariantChecker->printViolations(errs());
          return false;
        }
      }
    }
    
    return true;
  }
};
```

---

## 编译选项支持

```bash
--fcheck-invariants            启用不变量检查（Debug 构建默认启用）
--fcheck-invariants-fail-fast  发现违规立即中止
--fcheck-ssa                   仅检查 SSA 不变量
--fcheck-types                 仅检查类型不变量
--fcheck-cf                    仅检查控制流不变量
```

---

## 实现约束

1. **性能**：不变量检查仅在 Debug 构建或显式启用时运行，Release 构建零开销
2. **完整性**：覆盖所有关键不变量（SSA/类型/控制流/内存）
3. **可诊断性**：违规报告包含足够的上下文信息（Value/Instruction/BB/Function）
4. **可配置性**：可单独启用/禁用每类不变量检查

---

## 验收标准

### 单元测试

```cpp
// tests/unit/IR/PassInvariantCheckerTest.cpp

#include "gtest/gtest.h"
#include "blocktype/IR/PassInvariantChecker.h"
#include "blocktype/IR/IRBuilder.h"

using namespace blocktype;

TEST(PassInvariantChecker, SSA_SingleDefinition) {
  ir::IRContext Ctx;
  ir::IRTypeContext& TypeCtx = Ctx.getTypeContext();
  ir::IRModule M("test", TypeCtx);
  
  // 构造合法 IR
  auto* FTy = TypeCtx.getFunctionType(TypeCtx.getInt32Ty(), {});
  auto* F = M.getOrInsertFunction("test", FTy);
  auto* Entry = F->addBasicBlock("entry");
  
  ir::IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createRet(Builder.getInt32(42));
  
  ir::PassInvariantChecker Checker;
  EXPECT_TRUE(Checker.checkSingleDefinition(*F));
  EXPECT_FALSE(Checker.hasViolations());
}

TEST(PassInvariantChecker, Type_Completeness) {
  ir::IRContext Ctx;
  ir::IRTypeContext& TypeCtx = Ctx.getTypeContext();
  ir::IRModule M("test", TypeCtx);
  
  // 构造含 OpaqueType 的 IR
  auto* OpaqueTy = TypeCtx.getOpaqueType("unknown");
  auto* FTy = TypeCtx.getFunctionType(OpaqueTy, {});
  auto* F = M.getOrInsertFunction("test", FTy);
  
  ir::PassInvariantChecker Checker;
  EXPECT_FALSE(Checker.checkTypeCompleteness(M));
  EXPECT_TRUE(Checker.hasViolations());
  
  const auto& V = Checker.getViolations();
  EXPECT_EQ(V.size(), 1u);
  EXPECT_EQ(V[0].Kind, ir::InvariantKind::Type_Completeness);
}

TEST(PassInvariantChecker, CF_SingleTerminator) {
  ir::IRContext Ctx;
  ir::IRTypeContext& TypeCtx = Ctx.getTypeContext();
  ir::IRModule M("test", TypeCtx);
  
  auto* FTy = TypeCtx.getFunctionType(TypeCtx.getInt32Ty(), {});
  auto* F = M.getOrInsertFunction("test", FTy);
  auto* Entry = F->addBasicBlock("entry");
  
  ir::IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createRet(Builder.getInt32(42));
  
  ir::PassInvariantChecker Checker;
  EXPECT_TRUE(Checker.checkSingleTerminator(*F));
  EXPECT_FALSE(Checker.hasViolations());
}

TEST(PassInvariantChecker, CF_NoTerminator) {
  ir::IRContext Ctx;
  ir::IRTypeContext& TypeCtx = Ctx.getTypeContext();
  ir::IRModule M("test", TypeCtx);
  
  auto* FTy = TypeCtx.getFunctionType(TypeCtx.getInt32Ty(), {});
  auto* F = M.getOrInsertFunction("test", FTy);
  F->addBasicBlock("entry");  // 空 BB，无终结指令
  
  ir::PassInvariantChecker Checker;
  EXPECT_FALSE(Checker.checkSingleTerminator(*F));
  EXPECT_TRUE(Checker.hasViolations());
}

TEST(PassInvariantChecker, AllInvariants) {
  ir::IRContext Ctx;
  ir::IRTypeContext& TypeCtx = Ctx.getTypeContext();
  ir::IRModule M("test", TypeCtx);
  
  // 构造合法 IR
  auto* FTy = TypeCtx.getFunctionType(TypeCtx.getInt32Ty(), {});
  auto* F = M.getOrInsertFunction("test", FTy);
  auto* Entry = F->addBasicBlock("entry");
  
  ir::IRBuilder Builder(Ctx);
  Builder.setInsertPoint(Entry);
  Builder.createRet(Builder.getInt32(42));
  
  ir::PassInvariantChecker Checker;
  EXPECT_TRUE(Checker.checkAllInvariants(M));
  EXPECT_FALSE(Checker.hasViolations());
}
```

### 集成测试

```bash
# V1: 启用不变量检查
# blocktype --fcheck-invariants test.cpp -o test
# 如果 Pass 破坏不变量，输出违规报告

# V2: Fail-fast 模式
# blocktype --fcheck-invariants-fail-fast test.cpp -o test
# 发现违规立即中止

# V3: 仅检查 SSA
# blocktype --fcheck-ssa test.cpp -o test

# V4: 仅检查类型
# blocktype --fcheck-types test.cpp -o test

# V5: 仅检查控制流
# blocktype --fcheck-cf test.cpp -o test
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F9 — PassInvariantChecker 基础实现（SSA/类型不变量）" && git push origin HEAD
```
