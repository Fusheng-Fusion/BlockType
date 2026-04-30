# AI Coder 可执行任务流 — Phase B 增强任务 B-F6：StructuredDiagEmitter 基础实现（文本+JSON）

> 本文档是 AI coder 可直接执行的改造任务流。每个 Task 自包含所有必要信息：
> 接口签名、类型定义、参数约束、验收命令。AI coder 无需查阅其他文档即可编码。

---

## 执行规则

1. **严格按 Task 编号顺序执行**，每个 Task 完成并通过验收后再开始下一个
2. **接口签名不可修改**——本文档中的 class/struct/enum/函数签名是硬约束
3. **验收标准必须全部通过**——验收代码是可执行的断言
4. **命名空间**：代码在 `namespace blocktype::diag` 中
5. **头文件路径**：`include/blocktype/IR/`，源文件路径：`src/IR/`
6. **Git 提交与推送**：每个 Task 完成并通过验收后，**必须立即**执行以下操作：
   ```bash
   git add -A
   git commit -m "feat(B): 完成 B-F6 — StructuredDiagEmitter 基础实现（文本+JSON）"
   git push origin HEAD
   ```

---

## Task B-F6：StructuredDiagEmitter 基础实现（文本+JSON）

### 依赖验证

✅ **A-F6（StructuredDiagnostic 基础结构）**：已存在于 `include/blocktype/IR/IRDiagnostic.h`

### 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `include/blocktype/IR/IRDiagnostic.h` |
| 修改 | `src/IR/IRDiagnostic.cpp` |

---

## 必须实现的类型定义

### 0. 扩展 StructuredDiagnostic 结构（前置修改）

```cpp
// 在 IRDiagnostic.h 中扩展 StructuredDiagnostic

struct StructuredDiagnostic {
  DiagnosticLevel Level = DiagnosticLevel::Note;
  DiagnosticGroup Group = DiagnosticGroup::TypeMapping;
  DiagnosticCode  Code  = DiagnosticCode::Unknown;
  std::string Message;
  ir::SourceLocation Loc;
  ir::SmallVector<std::string, 4> Notes;
  
  // === 新增字段 ===
  std::string Category;                              // 诊断类别
  std::string FlagName;                              // 命令行标志名（如 "ir"）
  ir::SmallVector<ir::SourceRange, 4> Ranges;        // 源码范围
  ir::SmallVector<ir::SourceRange, 2> RelatedLocs;   // 相关位置
  
  // FixIt 提示
  struct FixItHint {
    ir::SourceRange Range;
    std::string Replacement;
    std::string Description;
  };
  ir::SmallVector<FixItHint, 2> FixIts;
  
  // IR 相关信息
  std::optional<ir::dialect::DialectID> IRRelatedDialect;
  std::optional<ir::Opcode> IRRelatedOpcode;
  
  // === 现有方法 ===
  DiagnosticLevel getLevel() const { return Level; }
  DiagnosticGroup getGroup() const { return Group; }
  DiagnosticCode  getCode()  const { return Code;  }
  ir::StringRef getMessage() const { return Message; }
  const ir::SourceLocation& getLocation() const { return Loc; }
  
  void addNote(ir::StringRef N) { Notes.push_back(N.str()); }
  
  void setLocation(ir::StringRef Filename, unsigned Line, unsigned Column) {
    Loc.Filename = Filename;
    Loc.Line = Line;
    Loc.Column = Column;
  }
  
  std::string toJSON() const;
  std::string toText() const;
};
```

### 1. TextDiagEmitter（文本格式发射器）

```cpp
// 在 IRDiagnostic.h 中添加

namespace blocktype {
namespace diag {

/// 文本格式诊断发射器（兼容 Clang 传统格式）
class TextDiagEmitter : public StructuredDiagEmitter {
  ir::raw_ostream& OS;
  bool ColorsEnabled;
  
public:
  explicit TextDiagEmitter(ir::raw_ostream& OS, bool Colors = false)
    : OS(OS), ColorsEnabled(Colors) {}
  
  void emit(const StructuredDiagnostic& D) override;
  
private:
  void emitLevel(ir::raw_ostream& OS, DiagnosticLevel L);
  void emitLocation(ir::raw_ostream& OS, const ir::SourceLocation& Loc);
  void emitMessage(ir::raw_ostream& OS, ir::StringRef Msg);
  void emitNotes(ir::raw_ostream& OS, const ir::SmallVector<std::string, 4>& Notes);
  void emitFixIts(ir::raw_ostream& OS, const ir::SmallVector<StructuredDiagnostic::FixItHint, 2>& FixIts);
};

} // namespace diag
} // namespace blocktype
```

### 2. JSONDiagEmitter（JSON 格式发射器）

```cpp
// 在 IRDiagnostic.h 中添加

namespace blocktype {
namespace diag {

/// JSON 格式诊断发射器（机器可读）
class JSONDiagEmitter : public StructuredDiagEmitter {
  ir::raw_ostream& OS;
  bool PrettyPrint;
  
public:
  explicit JSONDiagEmitter(ir::raw_ostream& OS, bool Pretty = false)
    : OS(OS), PrettyPrint(Pretty) {}
  
  void emit(const StructuredDiagnostic& D) override;
  
private:
  void emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, ir::StringRef Value, bool Last);
  void emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, unsigned Value, bool Last);
  void emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, bool Value, bool Last);
};

} // namespace diag
} // namespace blocktype
```

### 3. SARIFDiagEmitter（SARIF 格式发射器，远期）

```cpp
// 在 IRDiagnostic.h 中添加

namespace blocktype {
namespace diag {

/// SARIF 格式诊断发射器（Static Analysis Results Interchange Format）
/// 用于与 GitHub Code Scanning、VS Code 等工具集成
class SARIFDiagEmitter : public StructuredDiagEmitter {
  ir::raw_ostream& OS;
  
public:
  explicit SARIFDiagEmitter(ir::raw_ostream& OS) : OS(OS) {}
  
  void emit(const StructuredDiagnostic& D) override;
  
  /// 发射完整的 SARIF 报告（包含所有诊断）
  void emitSARIFReport(const ir::SmallVector<StructuredDiagnostic, 16>& Diags);
};

} // namespace diag
} // namespace blocktype
```

---

## 实现细节

### TextDiagEmitter::emit() 实现

```cpp
void TextDiagEmitter::emit(const StructuredDiagnostic& D) {
  // 格式：<location>: <level>: <message> [-W<flag>]
  // 示例：test.cpp:10:5: error: cannot map type 'auto' to IRType [-Wir]
  
  emitLocation(OS, D.getLocation());
  OS << ": ";
  emitLevel(OS, D.getLevel());
  OS << ": ";
  emitMessage(OS, D.getMessage());
  
  if (!D.FlagName.empty()) {
    OS << " [-W" << D.FlagName << "]";
  }
  OS << "\n";
  
  // 发射 Notes
  emitNotes(OS, D.Notes);
  
  // 发射 FixIts
  emitFixIts(OS, D.FixIts);
  
  OS.flush();
}

void TextDiagEmitter::emitLocation(ir::raw_ostream& OS, const ir::SourceLocation& Loc) {
  if (Loc.isValid()) {
    OS << Loc.Filename << ":" << Loc.Line << ":" << Loc.Column;
  } else {
    OS << "<unknown>";
  }
}

void TextDiagEmitter::emitLevel(ir::raw_ostream& OS, DiagnosticLevel L) {
  if (ColorsEnabled) {
    switch (L) {
      case DiagnosticLevel::Note:    OS << "\033[1;34mnote\033[0m"; break;    // 蓝色
      case DiagnosticLevel::Remark:  OS << "\033[1;36mremark\033[0m"; break;  // 青色
      case DiagnosticLevel::Warning: OS << "\033[1;33mwarning\033[0m"; break;  // 黄色
      case DiagnosticLevel::Error:   OS << "\033[1;31merror\033[0m"; break;    // 红色
      case DiagnosticLevel::Fatal:   OS << "\033[1;35mfatal error\033[0m"; break; // 紫色
    }
  } else {
    switch (L) {
      case DiagnosticLevel::Note:    OS << "note"; break;
      case DiagnosticLevel::Remark:  OS << "remark"; break;
      case DiagnosticLevel::Warning: OS << "warning"; break;
      case DiagnosticLevel::Error:   OS << "error"; break;
      case DiagnosticLevel::Fatal:   OS << "fatal error"; break;
    }
  }
}

void TextDiagEmitter::emitMessage(ir::raw_ostream& OS, ir::StringRef Msg) {
  OS << Msg;
}

void TextDiagEmitter::emitNotes(ir::raw_ostream& OS, const ir::SmallVector<std::string, 4>& Notes) {
  for (const auto& Note : Notes) {
    OS << "  " << Note << "\n";
  }
}

void TextDiagEmitter::emitFixIts(ir::raw_ostream& OS, const ir::SmallVector<StructuredDiagnostic::FixItHint, 2>& FixIts) {
  for (const auto& FI : FixIts) {
    OS << "  FIX-IT: " << FI.Description << "\n";
    OS << "    Replace with: " << FI.Replacement << "\n";
  }
}
```

### JSONDiagEmitter::emit() 实现

```cpp
void JSONDiagEmitter::emit(const StructuredDiagnostic& D) {
  OS << "{";
  
  // level
  emitJSONField(OS, "level", getDiagnosticLevelName(D.getLevel()), false);
  
  // code
  emitJSONField(OS, "code", getDiagnosticCodeName(D.getCode()), false);
  
  // message
  emitJSONField(OS, "message", D.getMessage(), false);
  
  // location
  OS << "\"location\": {";
  emitJSONField(OS, "file", D.getLocation().Filename, false);
  emitJSONField(OS, "line", D.getLocation().Line, false);
  emitJSONField(OS, "column", D.getLocation().Column, true);
  OS << "}, ";
  
  // category
  if (!D.Category.empty()) {
    emitJSONField(OS, "category", D.Category, false);
  }
  
  // flag
  if (!D.FlagName.empty()) {
    emitJSONField(OS, "flag", D.FlagName, false);
  }
  
  // IR-related info
  if (D.IRRelatedDialect.has_value()) {
    emitJSONField(OS, "ir_dialect", 
                  ir::dialect::getDialectName(*D.IRRelatedDialect), false);
  }
  
  if (D.IRRelatedOpcode.has_value()) {
    emitJSONField(OS, "ir_opcode", 
                  ir::getOpcodeName(*D.IRRelatedOpcode), false);
  }
  
  // notes
  if (!D.Notes.empty()) {
    OS << "\"notes\": [";
    for (size_t i = 0; i < D.Notes.size(); ++i) {
      OS << "\"" << D.Notes[i] << "\"";
      if (i < D.Notes.size() - 1) OS << ", ";
    }
    OS << "], ";
  }
  
  // fixits
  if (!D.FixIts.empty()) {
    OS << "\"fixits\": [";
    for (size_t i = 0; i < D.FixIts.size(); ++i) {
      const auto& FI = D.FixIts[i];
      OS << "{\"description\": \"" << FI.Description << "\", ";
      OS << "\"replacement\": \"" << FI.Replacement << "\"}";
      if (i < D.FixIts.size() - 1) OS << ", ";
    }
    OS << "]";
  } else {
    OS << "\"fixits\": []";
  }
  
  OS << "}\n";
  OS.flush();
}

void JSONDiagEmitter::emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, ir::StringRef Value, bool Last) {
  OS << "\"" << Key << "\": \"" << Value << "\"";
  if (!Last) OS << ", ";
}

void JSONDiagEmitter::emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, unsigned Value, bool Last) {
  OS << "\"" << Key << "\": " << Value;
  if (!Last) OS << ", ";
}

void JSONDiagEmitter::emitJSONField(ir::raw_ostream& OS, ir::StringRef Key, bool Value, bool Last) {
  OS << "\"" << Key << "\": " << (Value ? "true" : "false");
  if (!Last) OS << ", ";
}
```

---

## 输出格式示例

### 文本格式

```
test.cpp:10:5: error: cannot map QualType 'auto' to IRType [-Wir]
  note: type depends on template parameter
  FIX-IT: Replace with explicit type
    Replace with: int
```

### JSON 格式

```json
{
  "level": "error",
  "code": "TypeMappingFailed",
  "message": "cannot map QualType 'auto' to IRType",
  "location": {
    "file": "test.cpp",
    "line": 10,
    "column": 5
  },
  "category": "type-error",
  "flag": "ir",
  "ir_dialect": "Cpp",
  "notes": ["type depends on template parameter"],
  "fixits": [
    {
      "description": "Replace with explicit type",
      "replacement": "int"
    }
  ]
}
```

---

## 实现约束

1. **线程安全**：所有 emit() 方法不修改全局状态，可并发调用
2. **性能**：文本格式零开销（直接输出），JSON 格式最小化字符串拷贝
3. **扩展性**：新增输出格式只需继承 StructuredDiagEmitter
4. **兼容性**：文本格式与 Clang 传统格式兼容

---

## 验收标准

### 单元测试

```cpp
// tests/unit/IR/StructuredDiagEmitterTest.cpp

#include "gtest/gtest.h"
#include "blocktype/IR/IRDiagnostic.h"
#include <sstream>

using namespace blocktype;

TEST(TextDiagEmitter, BasicEmission) {
  std::string Output;
  ir::raw_string_ostream OS(Output);
  diag::TextDiagEmitter Emitter(OS);
  
  diag::StructuredDiagnostic D;
  D.Level = diag::DiagnosticLevel::Error;
  D.Code = diag::DiagnosticCode::TypeMappingFailed;
  D.Message = "cannot map type";
  D.setLocation("test.cpp", 10, 5);
  
  Emitter.emit(D);
  
  EXPECT_TRUE(Output.find("test.cpp:10:5") != std::string::npos);
  EXPECT_TRUE(Output.find("error") != std::string::npos);
  EXPECT_TRUE(Output.find("cannot map type") != std::string::npos);
}

TEST(TextDiagEmitter, ColorOutput) {
  std::string Output;
  ir::raw_string_ostream OS(Output);
  diag::TextDiagEmitter Emitter(OS, true);  // 启用颜色
  
  diag::StructuredDiagnostic D;
  D.Level = diag::DiagnosticLevel::Error;
  D.Message = "test";
  
  Emitter.emit(D);
  
  // 验证 ANSI 颜色码
  EXPECT_TRUE(Output.find("\033[1;31m") != std::string::npos);  // 红色
  EXPECT_TRUE(Output.find("\033[0m") != std::string::npos);     // 重置
}

TEST(JSONDiagEmitter, BasicEmission) {
  std::string Output;
  ir::raw_string_ostream OS(Output);
  diag::JSONDiagEmitter Emitter(OS);
  
  diag::StructuredDiagnostic D;
  D.Level = diag::DiagnosticLevel::Error;
  D.Code = diag::DiagnosticCode::TypeMappingFailed;
  D.Message = "cannot map type";
  D.setLocation("test.cpp", 10, 5);
  D.Category = "type-error";
  
  Emitter.emit(D);
  
  EXPECT_TRUE(Output.find("\"level\": \"error\"") != std::string::npos);
  EXPECT_TRUE(Output.find("\"code\": \"TypeMappingFailed\"") != std::string::npos);
  EXPECT_TRUE(Output.find("\"message\": \"cannot map type\"") != std::string::npos);
  EXPECT_TRUE(Output.find("\"file\": \"test.cpp\"") != std::string::npos);
  EXPECT_TRUE(Output.find("\"line\": 10") != std::string::npos);
}

TEST(JSONDiagEmitter, WithNotesAndFixIts) {
  std::string Output;
  ir::raw_string_ostream OS(Output);
  diag::JSONDiagEmitter Emitter(OS);
  
  diag::StructuredDiagnostic D;
  D.Message = "test";
  D.addNote("note 1");
  D.addNote("note 2");
  
  diag::StructuredDiagnostic::FixItHint FI;
  FI.Description = "fix description";
  FI.Replacement = "int";
  D.FixIts.push_back(FI);
  
  Emitter.emit(D);
  
  EXPECT_TRUE(Output.find("\"notes\"") != std::string::npos);
  EXPECT_TRUE(Output.find("note 1") != std::string::npos);
  EXPECT_TRUE(Output.find("\"fixits\"") != std::string::npos);
  EXPECT_TRUE(Output.find("fix description") != std::string::npos);
}

TEST(JSONDiagEmitter, IRRelatedInfo) {
  std::string Output;
  ir::raw_string_ostream OS(Output);
  diag::JSONDiagEmitter Emitter(OS);
  
  diag::StructuredDiagnostic D;
  D.Message = "test";
  D.IRRelatedDialect = ir::dialect::DialectID::Cpp;
  D.IRRelatedOpcode = ir::Opcode::Call;
  
  Emitter.emit(D);
  
  EXPECT_TRUE(Output.find("\"ir_dialect\": \"Cpp\"") != std::string::npos);
  EXPECT_TRUE(Output.find("\"ir_opcode\": \"Call\"") != std::string::npos);
}
```

### 集成测试

```bash
# V1: 文本格式输出
# blocktype test.cpp 2>&1 | grep "error:"

# V2: JSON 格式输出
# blocktype --fdiagnostics-format=json test.cpp 2>&1 | jq '.level'

# V3: 验证 JSON 格式有效
# blocktype --fdiagnostics-format=json test.cpp 2>&1 | jq '.' > /dev/null
```

---

## Git 提交提醒

本 Task 完成后，立即执行：
```bash
git add -A && git commit -m "feat(B): 完成 B-F6 — StructuredDiagEmitter 基础实现（文本+JSON）" && git push origin HEAD
```
