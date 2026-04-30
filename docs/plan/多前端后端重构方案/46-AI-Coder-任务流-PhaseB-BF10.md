# AI Coder 可执行任务流 — Phase B 增强任务 B-F10：IR调试信息：前端→IR调试元数据传递

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype::frontend` 和 `namespace blocktype::ir` 中
5. **头文件路径**：`include/blocktype/Frontend/` 和 `include/blocktype/IR/`，源文件路径：`src/Frontend/` 和 `src/IR/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F10 — IR调试信息：前端→IR调试元数据传递"
   git push origin HEAD
   ```

---

## Task B-F10：IR调试信息：前端→IR调试元数据传递

### 依赖验证

✅ **A-F4（IRInstruction 添加 Optional DbgInfo 字段）**：已存在于 `include/blocktype/IR/IRInstruction.h`
✅ **A-F5（IRDebugMetadata 基础类型定义）**：已存在于 `include/blocktype/IR/IRDebugMetadata.h`
✅ **A-F5（IRInstructionDebugInfo 完整类型）**：已存在于 `include/blocktype/IR/IRDebugInfo.h`

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Frontend/IRDebugInfoEmitter.h` |
| 新增 | `src/Frontend/IRDebugInfoEmitter.cpp` |
| 修改 | `src/Frontend/ASTToIRConverter.cpp` |

---

## 必须实现的类型定义

### 1. IRDebugInfoEmitter 类

```cpp
// 在 IRDebugInfoEmitter.h 中定义

#ifndef BLOCKTYPE_FRONTEND_IRDEBUGINFOEMITTER_H
#define BLOCKTYPE_FRONTEND_IRDEBUGINFOEMITTER_H

#include "blocktype/IR/IRDebugInfo.h"
#include "blocktype/IR/IRDebugMetadata.h"
#include "blocktype/AST/ASTContext.h"
#include <memory>

namespace blocktype {
namespace frontend {

/// IR 调试信息发射器
/// 负责将 Clang AST 的调试信息转换为 IR 调试元数据
class IRDebugInfoEmitter {
  ir::IRModule& TheModule;
  SourceManager& SM;  // 使用 SourceManager 而非 ASTContext
  
  // 调试元数据缓存
  DenseMap<const Decl*, ir::DISubprogram*> SubprogramCache;
  DenseMap<const Type*, ir::DIType*> TypeCache;
  std::unique_ptr<ir::DICompileUnit> CU;
  
public:
  IRDebugInfoEmitter(ir::IRModule& M, SourceManager& S);
  
  // === 编译单元 ===
  
  /// 创建编译单元调试信息
  void emitCompileUnit(const TranslationUnitDecl* TU);
  
  // === 函数 ===
  
  /// 为函数创建调试信息
  ir::DISubprogram* emitFunctionDebugInfo(const FunctionDecl* FD);
  
  /// 为 C++ 方法创建调试信息
  ir::DISubprogram* emitCXXMethodDebugInfo(const CXXMethodDecl* MD);
  
  // === 类型 ===
  
  /// 为类型创建调试信息
  ir::DIType* emitTypeDebugInfo(QualType T);
  
  /// 为 Record 类型创建调试信息
  ir::DIType* emitRecordDebugInfo(const RecordDecl* RD);
  
  /// 为 Enum 类型创建调试信息
  ir::DIType* emitEnumDebugInfo(const EnumDecl* ED);
  
  // === 指令级调试信息 ===
  
  /// 为指令创建调试信息
  ir::debug::IRInstructionDebugInfo emitInstructionDebugInfo(const Stmt* S);
  
  /// 设置指令的调试信息
  void setInstructionDebugInfo(ir::IRInstruction* I, const Stmt* S);
  
  /// 设置指令的源码位置
  void setInstructionLocation(ir::IRInstruction* I, SourceLocation Loc);
  
  // === 内联信息 ===
  
  /// 标记指令为内联
  void markInlined(ir::IRInstruction* I, SourceLocation InlinedAt);
  
  // === 辅助方法 ===
  
  /// 将 Clang SourceLocation 转换为 IR SourceLocation
  ir::SourceLocation convertSourceLocation(SourceLocation Loc);
  
  /// 获取或创建 DISubprogram
  ir::DISubprogram* getOrCreateSubprogram(const FunctionDecl* FD);
  
  /// 获取或创建 DIType
  ir::DIType* getOrCreateDIType(QualType T);
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_IRDEBUGINFOEMITTER_H
```

---

## 实现细节

### 1. emitCompileUnit 实现

```cpp
void IRDebugInfoEmitter::emitCompileUnit(const TranslationUnitDecl* TU) {
  // 创建编译单元调试信息
  CU = std::make_unique<ir::DICompileUnit>();
  
  // 设置源文件路径
  if (const auto* MainFile = SM.getFileEntryForID(SM.getMainFileID())) {
    CU->setSourceFile(MainFile->getName());
  }
  
  // 设置生产者信息
  CU->setProducer("BlockType Compiler 1.0.0");
  
  // 设置语言（C++ = 4，根据 DWARF 语言编码）
  CU->setLanguage(4);  // DW_LANG_C_plus_plus
  
  // 将编译单元添加到模块元数据
  TheModule.addMetadata(std::move(CU));
}
```

### 2. emitFunctionDebugInfo 实现

```cpp
ir::DISubprogram* IRDebugInfoEmitter::emitFunctionDebugInfo(const FunctionDecl* FD) {
  // 检查缓存
  auto It = SubprogramCache.find(FD);
  if (It != SubprogramCache.end()) {
    return It->second;
  }
  
  // 创建 DISubprogram
  auto* SP = new ir::DISubprogram(FD->getNameAsString());
  
  // 设置编译单元
  if (CU) {
    SP->setUnit(CU.get());
  }
  
  // 设置源码位置
  SourceLocation Loc = FD->getLocation();
  ir::SourceLocation IRLoc = convertSourceLocation(Loc);
  
  // 缓存
  SubprogramCache[FD] = SP;
  
  return SP;
}
```

### 3. emitInstructionDebugInfo 实现

```cpp
ir::debug::IRInstructionDebugInfo IRDebugInfoEmitter::emitInstructionDebugInfo(const Stmt* S) {
  ir::debug::IRInstructionDebugInfo DI;
  
  if (!S) return DI;
  
  // 设置源码位置
  SourceLocation Loc = S->getBeginLoc();
  ir::SourceLocation IRLoc = convertSourceLocation(Loc);
  DI.setLocation(IRLoc);
  
  // 设置所属子程序（如果语句在函数内）
  if (const auto* FD = dyn_cast<FunctionDecl>(S->getDeclContext())) {
    auto* SP = getOrCreateSubprogram(FD);
    DI.setSubprogram(SP);
  }
  
  // 标记为人工生成（如果是编译器生成的代码）
  if (S->isImplicit()) {
    DI.setArtificial(true);
  }
  
  return DI;
}
```

### 4. setInstructionDebugInfo 实现

```cpp
void IRDebugInfoEmitter::setInstructionDebugInfo(ir::IRInstruction* I, const Stmt* S) {
  if (!I || !S) return;
  
  auto DI = emitInstructionDebugInfo(S);
  I->setDebugInfo(DI);
}
```

### 5. convertSourceLocation 实现

```cpp
ir::SourceLocation IRDebugInfoEmitter::convertSourceLocation(SourceLocation Loc) {
  ir::SourceLocation IRLoc;
  
  if (Loc.isInvalid()) {
    return IRLoc;
  }
  
  // 获取文件名
  if (const auto* FE = SM.getFileEntryForID(SM.getFileID(Loc))) {
    IRLoc.Filename = FE->getName();
  }
  
  // 获取行号和列号
  IRLoc.Line = SM.getLineNumber(SM.getFileID(Loc), SM.getFileOffset(Loc));
  IRLoc.Column = SM.getColumnNumber(SM.getFileID(Loc), SM.getFileOffset(Loc));
  
  return IRLoc;
}
```

---

## 与 ASTToIRConverter 集成

### 1. 在 ASTToIRConverter 中添加 DebugInfoEmitter

```cpp
// 在 ASTToIRConverter.h 中添加

class ASTToIRConverter {
  // ... 现有成员 ...
  
  /// Debug info emitter (optional, created when -g is enabled)
  unique_ptr<IRDebugInfoEmitter> DebugEmitter;
  
public:
  // === 调试信息接口 ===
  
  /// Create debug info emitter
  void createDebugEmitter();
  
  /// Check if debug info is enabled
  bool hasDebugInfo() const { return DebugEmitter != nullptr; }
  
  /// Get debug info emitter
  IRDebugInfoEmitter* getDebugEmitter() { return DebugEmitter.get(); }
};
```

### 2. 在 convert() 中创建编译单元

```cpp
ir::IRConversionResult ASTToIRConverter::convert(TranslationUnitDecl* TU) {
  // ... 创建 IRModule ...
  
  // 如果启用调试信息，创建编译单元
  if (hasDebugInfo()) {
    DebugEmitter->emitCompileUnit(TU);
  }
  
  // ... 转换函数和全局变量 ...
  
  for (auto* D : TU->decls()) {
    if (auto* FD = dyn_cast<FunctionDecl>(D)) {
      emitFunction(FD);
    } else if (auto* VD = dyn_cast<VarDecl>(D)) {
      if (VD->hasGlobalStorage()) {
        emitGlobalVar(VD);
      }
    }
  }
  
  // ...
}
```

### 3. 在 emitFunction() 中创建函数调试信息

```cpp
ir::IRFunction* ASTToIRConverter::emitFunction(FunctionDecl* FD) {
  // ... 创建 IRFunction ...
  
  // 如果启用调试信息，创建函数调试信息
  if (hasDebugInfo()) {
    auto* SP = DebugEmitter->emitFunctionDebugInfo(FD);
    // 将 SP 关联到 IRFunction（如果 IRFunction 支持元数据）
  }
  
  // ... 转换函数体 ...
  
  if (FD->hasBody()) {
    emitFunctionBody(FD, IRFn);
  }
  
  return IRFn;
}
```

### 4. 在 IREmitStmt 中设置指令调试信息

```cpp
void IREmitStmt::EmitCompoundStmt(const CompoundStmt* CS) {
  for (auto* S : CS->body()) {
    // 发射语句
    EmitStmt(S);
    
    // 如果启用调试信息，设置最后一条指令的调试信息
    if (Converter.hasDebugInfo() && Builder.getLastInstruction()) {
      Converter.getDebugEmitter()->setInstructionDebugInfo(
        Builder.getLastInstruction(), S);
    }
  }
}
```

---

## 编译选项支持

```bash
-g                             生成调试信息（DWARF5 默认）
-gdwarf-4                      生成 DWARF4 调试信息
-gdwarf-5                      生成 DWARF5 调试信息
-gcodeview                     生成 CodeView 调试信息（Windows）
-gir-debug                     在 IR 中保留调试元数据
-fdebug-info-for-profiling     生成性能剖析用调试信息
```

### 选项解析

```cpp
// 在 CompilerInvocation 中添加

if (Args.hasArg(OPT_g)) {
  Opts.DebugInfo = true;
  Opts.DebugFormat = DebugFormatKind::DWARF5;
}

if (Args.hasArg(OPT_gdwarf_4)) {
  Opts.DebugInfo = true;
  Opts.DebugFormat = DebugFormatKind::DWARF4;
}

if (Args.hasArg(OPT_gdwarf_5)) {
  Opts.DebugInfo = true;
  Opts.DebugFormat = DebugFormatKind::DWARF5;
}

if (Args.hasArg(OPT_gcodeview)) {
  Opts.DebugInfo = true;
  Opts.DebugFormat = DebugFormatKind::CodeView;
}

if (Args.hasArg(OPT_gir_debug)) {
  Opts.IRDebugInfo = true;
}

if (Args.hasArg(OPT_fdebug_info_for_profiling)) {
  Opts.DebugInfoForProfiling = true;
}
```

---

## 调试信息传递流程

```
Clang AST SourceLocation
        ↓
IRDebugInfoEmitter::convertSourceLocation()
        ↓
ir::SourceLocation (Filename, Line, Column)
        ↓
ir::debug::IRInstructionDebugInfo
        ↓
IRInstruction::setDebugInfo()
        ↓
IRModule (携带调试元数据)
        ↓
Backend (DWARF/CodeView 发射器)
```

---

## 实现约束

1. **可选性**：调试信息生成是可选的，未启用时零开销
2. **完整性**：支持函数/类型/指令级调试信息
3. **兼容性**：支持 DWARF4/DWARF5/CodeView 多种格式
4. **内存管理**：DISubprogram/DIType 由 IRModule 持有，随模块释放
5. **内联支持**：支持内联函数的调试信息传递

---

## 验收标准

### 单元测试

```cpp
// tests/unit/Frontend/IRDebugInfoEmitterTest.cpp

#include "gtest/gtest.h"
#include "blocktype/Frontend/IRDebugInfoEmitter.h"
#include "blocktype/IR/IRBuilder.h"

using namespace blocktype;

TEST(IRDebugInfoEmitter, ConvertSourceLocation) {
  // 需要 ASTContext 和 SourceManager
  // 此处为示例，实际测试需要完整的 AST 环境
  
  ir::SourceLocation Loc;
  Loc.Filename = "test.cpp";
  Loc.Line = 10;
  Loc.Column = 5;
  
  EXPECT_TRUE(Loc.isValid());
  EXPECT_EQ(Loc.Filename, "test.cpp");
  EXPECT_EQ(Loc.Line, 10u);
  EXPECT_EQ(Loc.Column, 5u);
}

TEST(IRDebugInfoEmitter, InstructionDebugInfo) {
  ir::debug::IRInstructionDebugInfo DI;
  
  EXPECT_FALSE(DI.hasLocation());
  
  ir::SourceLocation Loc;
  Loc.Filename = "test.cpp";
  Loc.Line = 10;
  Loc.Column = 5;
  
  DI.setLocation(Loc);
  EXPECT_TRUE(DI.hasLocation());
  EXPECT_EQ(DI.getLocation().Filename, "test.cpp");
  EXPECT_EQ(DI.getLocation().Line, 10u);
}

TEST(IRDebugInfoEmitter, ArtificialFlag) {
  ir::debug::IRInstructionDebugInfo DI;
  
  EXPECT_FALSE(DI.isArtificial());
  
  DI.setArtificial(true);
  EXPECT_TRUE(DI.isArtificial());
}

TEST(IRDebugInfoEmitter, InlinedInfo) {
  ir::debug::IRInstructionDebugInfo DI;
  
  EXPECT_FALSE(DI.isInlined());
  EXPECT_FALSE(DI.hasInlinedAt());
  
  DI.setInlined(true);
  EXPECT_TRUE(DI.isInlined());
  
  ir::SourceLocation InlinedAt;
  InlinedAt.Filename = "caller.cpp";
  InlinedAt.Line = 20;
  DI.setInlinedAt(InlinedAt);
  
  EXPECT_TRUE(DI.hasInlinedAt());
  EXPECT_EQ(DI.getInlinedAt().Filename, "caller.cpp");
  EXPECT_EQ(DI.getInlinedAt().Line, 20u);
}

TEST(IRDebugInfoEmitter, SubprogramAssociation) {
  ir::debug::IRInstructionDebugInfo DI;
  
  EXPECT_FALSE(DI.hasSubprogram());
  
  ir::DISubprogram SP("test_func");
  DI.setSubprogram(&SP);
  
  EXPECT_TRUE(DI.hasSubprogram());
  EXPECT_EQ(DI.getSubprogram()->getName(), "test_func");
}
```

### 集成测试

```bash
# V1: 生成调试信息
# blocktype -g test.cpp -o test
# gdb test
# (gdb) break main
# (gdb) run
# (gdb) list
# 应该显示源码

# V2: DWARF4 格式
# blocktype -gdwarf-4 test.cpp -o test
# readelf --debug-dump=test | grep "DWARF version"
# 输出: DWARF version: 4

# V3: DWARF5 格式
# blocktype -gdwarf-5 test.cpp -o test
# readelf --debug-dump=test | grep "DWARF version"
# 输出: DWARF version: 5

# V4: CodeView 格式（Windows）
# blocktype -gcodeview test.cpp -o test.exe
# dumpbin /HEADERS test.exe | findstr "Debug"
# 应该包含 CodeView 调试信息

# V5: IR 调试元数据
# blocktype -gir-debug --emit-btir test.cpp -o test.btir
# cat test.btir | grep "dbg"
# 应该包含调试元数据

# V6: 调试信息验证
# blocktype -g test.cpp -o test
# llvm-dwarfdump test | grep "DW_TAG_subprogram"
# 应该包含函数调试信息
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F10 — IR调试信息：前端→IR调试元数据传递" && git push origin HEAD
```
