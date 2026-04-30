# Task B-F2：StructuredDiagnostic 完整结构定义（优化版）

## 1. 代码库验证摘要（修正列表）

### 已验证类型

| 规格中的类型 | 实际代码库 | 状态 | 修正 |
|-------------|-----------|------|------|
| `diag::Level` | `diag::DiagnosticLevel`（`IR/IRDiagnostic.h:17`） | ❌ 不存在 | 使用 `diag::DiagnosticLevel` |
| `StructuredDiagnostic` | 已存在于 `IR/IRDiagnostic.h:82-106` | ⚠️ 冲突 | 扩展现有结构，新增字段 |
| `StructuredDiagEmitter` | 已存在于 `IR/IRDiagnostic.h:112-116` | ⚠️ 冲突 | 扩展现有类，新增方法 |
| `DiagnosticGroupManager` | 已存在于 `IR/IRDiagnostic.h:122-140` | ⚠️ 冲突 | 不重复创建，复用现有 |
| `SourceLocation` (Basic) | `blocktype::SourceLocation`（`Basic/SourceLocation.h`） | ✅ 存在 | 用 `blocktype::SourceLocation` |
| `SourceRange` (Basic) | `blocktype::SourceRange`（`Basic/SourceLocation.h`） | ✅ 存在 | 用 `blocktype::SourceRange` |
| `ir::SourceLocation` | `ir::SourceLocation`（`IR/IRDebugMetadata.h:16-29`） | ✅ 存在 | 两套 SourceLocation 共存 |
| `Optional<T>` | `std::optional<T>` | ✅ 存在 | 使用 `std::optional` |
| `ir::DialectID` | `ir::dialect::DialectID`（`IR/IRType.h:16`） | ⚠️ 命名空间 | 使用 `ir::dialect::DialectID` |
| `ir::Opcode` | `ir::Opcode`（`IR/IRValue.h:31`） | ✅ 存在 | 直接使用 |
| `SmallVector` | `ir::SmallVector`（`IR/ADT/SmallVector.h`） | ✅ 存在 | 使用 `ir::SmallVector` |
| `raw_ostream` | `ir::raw_ostream`（`IR/ADT/raw_ostream.h`） | ✅ 存在 | 使用 `ir::raw_ostream` |

### 关键修正

1. **`diag::Level` → `diag::DiagnosticLevel`**：规格中写的 `diag::Level::Error` 实际应为 `diag::DiagnosticLevel::Error`。
2. **StructuredDiagnostic 扩展策略**：不创建同名新结构体。在现有的 `StructuredDiagnostic`（`IR/IRDiagnostic.h:82`）基础上新增字段，保持已有字段 `Level`, `Group`, `Code`, `Message`, `Loc`, `Notes` 不变。
3. **StructuredDiagEmitter 扩展**：在现有抽象基类基础上新增 `emitSARIF()`/`emitJSON()`/`emitText()` 非虚方法（实现类中 override `emit()` 后调用这些方法）。
4. **DiagnosticGroupManager 复用**：现有类使用 `DiagnosticGroup` 枚举（不是 `StringRef`），保持不变。
5. **`blocktype::SourceLocation` vs `ir::SourceLocation`**：
   - `blocktype::SourceLocation`：紧凑 64 位编码（FileID + Offset），用于编译器内部
   - `ir::SourceLocation`：`{Filename, Line, Column}` 字符串形式，用于诊断输出
   - 新增字段 `PrimaryLoc` 使用 `blocktype::SourceLocation`（编译器定位用）
   - 现有字段 `Loc` 保持 `ir::SourceLocation`（诊断文本用）
6. **`ir::DialectID` → `ir::dialect::DialectID`**：DialectID 在 `ir::dialect` 命名空间中。
7. **`raw_string_ostream`**：使用 `llvm::raw_string_ostream`。

---

## 2. 完整类定义（精确 API 签名）

### 头文件：`include/blocktype/Frontend/StructuredDiagnostic.h`

```cpp
#ifndef BLOCKTYPE_FRONTEND_STRUCTUREDDIAGNOSTIC_H
#define BLOCKTYPE_FRONTEND_STRUCTUREDDIAGNOSTIC_H

#include <optional>
#include <string>

#include "blocktype/IR/ADT.h"
#include "blocktype/IR/IRDiagnostic.h"
#include "blocktype/IR/IRType.h"       // ir::dialect::DialectID
#include "blocktype/IR/IRValue.h"      // ir::Opcode
#include "blocktype/Basic/SourceLocation.h"

namespace blocktype {
namespace diag {

// ============================================================
// FixItHint — 修复提示
// ============================================================

struct FixItHint {
  blocktype::SourceRange Range;
  std::string Replacement;
  std::string Description;
};

// ============================================================
// DiagnosticNote — 带位置的附加说明
// ============================================================

struct DiagnosticNote {
  blocktype::SourceLocation Loc;
  std::string Message;
};

// ============================================================
// DetailedStructuredDiagnostic — 扩展诊断结构
// ============================================================
//
// 继承自现有的 StructuredDiagnostic（IR/IRDiagnostic.h），
// 新增 FixIt、SARIF、IR 关联等字段。
// 现有字段（Level/Group/Code/Message/Loc/Notes）保持不变。

struct DetailedStructuredDiagnostic : public StructuredDiagnostic {
  // 主定位（使用 Basic::SourceLocation 编码形式）
  blocktype::SourceLocation PrimaryLoc;

  // 高亮范围
  ir::SmallVector<blocktype::SourceRange, 4> Ranges;

  // 关联位置
  ir::SmallVector<blocktype::SourceRange, 2> RelatedLocs;

  // 分类标签（如 "type-error"、"syntax"）
  std::string Category;

  // 对应的警告标志名（如 "-Wir"、"-Wdialect"）
  std::string FlagName;

  // FixIt 提示列表
  ir::SmallVector<FixItHint, 2> FixIts;

  // 带位置的详细说明（替代基类 Notes 的纯字符串）
  ir::SmallVector<DiagnosticNote, 4> DetailedNotes;

  // IR 关联信息
  std::optional<ir::dialect::DialectID> IRRelatedDialect;
  std::optional<ir::Opcode> IRRelatedOpcode;
};

// ============================================================
// DetailedDiagEmitter — 扩展诊断发射器
// ============================================================
//
// 继承自 StructuredDiagEmitter，新增 SARIF/JSON/Text 输出。

class DetailedDiagEmitter : public StructuredDiagEmitter {
public:
  ~DetailedDiagEmitter() override = default;

  /// 发射诊断（实现基类纯虚方法）
  void emit(const StructuredDiagnostic& D) override;

  /// 将最近发射的诊断以 SARIF 格式输出
  void emitSARIF(ir::raw_ostream& OS) const;

  /// 将最近发射的诊断以 JSON 格式输出
  void emitJSON(ir::raw_ostream& OS) const;

  /// 将最近发射的诊断以纯文本格式输出
  void emitText(ir::raw_ostream& OS) const;

private:
  // 缓存最近一次诊断（用于格式化输出）
  DetailedStructuredDiagnostic LastDiag_;
  bool HasLast_ = false;
};

} // namespace diag
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_STRUCTUREDDIAGNOSTIC_H
```

### 实现文件：`src/Frontend/StructuredDiagnostic.cpp`

```cpp
#include "blocktype/Frontend/StructuredDiagnostic.h"

#include <sstream>

namespace blocktype {
namespace diag {

void DetailedDiagEmitter::emit(const StructuredDiagnostic& D) {
  // 将基类引用尝试转换为 DetailedStructuredDiagnostic
  // 存储副本用于后续 SARIF/JSON/Text 输出
  const auto* Detailed = dynamic_cast<const DetailedStructuredDiagnostic*>(&D);
  if (Detailed) {
    LastDiag_ = *Detailed;
    HasLast_ = true;
  }
}

void DetailedDiagEmitter::emitSARIF(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  // SARIF 2.1 简化格式
  OS << "{\"$schema\":\"https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json\",\n";
  OS << " \"runs\":[{\"results\":[{\n";
  OS << "   \"level\":\"" << getDiagnosticLevelName(LastDiag_.Level) << "\",\n";
  OS << "   \"message\":{\"text\":\"" << LastDiag_.Message << "\"},\n";
  if (!LastDiag_.Category.empty()) {
    OS << "   \"ruleId\":\"" << LastDiag_.Category << "\",\n";
  }
  OS << "   \"ruleIndex\":" << static_cast<uint32_t>(LastDiag_.Code) << "\n";
  OS << "  }]}\n]}\n";
}

void DetailedDiagEmitter::emitJSON(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  OS << "{\n";
  OS << "  \"level\": \"" << getDiagnosticLevelName(LastDiag_.Level) << "\",\n";
  OS << "  \"code\": " << static_cast<uint32_t>(LastDiag_.Code) << ",\n";
  OS << "  \"group\": \"" << getDiagnosticGroupName(LastDiag_.Group) << "\",\n";
  OS << "  \"message\": \"" << LastDiag_.Message << "\"";
  if (!LastDiag_.Category.empty()) {
    OS << ",\n  \"category\": \"" << LastDiag_.Category << "\"";
  }
  if (!LastDiag_.FlagName.empty()) {
    OS << ",\n  \"flag\": \"" << LastDiag_.FlagName << "\"";
  }
  if (LastDiag_.IRRelatedDialect.has_value()) {
    OS << ",\n  \"dialect\": " << static_cast<unsigned>(*LastDiag_.IRRelatedDialect);
  }
  if (LastDiag_.IRRelatedOpcode.has_value()) {
    OS << ",\n  \"opcode\": " << static_cast<unsigned>(*LastDiag_.IRRelatedOpcode);
  }
  if (!LastDiag_.FixIts.empty()) {
    OS << ",\n  \"fixits\": [";
    for (size_t i = 0; i < LastDiag_.FixIts.size(); ++i) {
      if (i > 0) OS << ", ";
      OS << "{\"replacement\":\"" << LastDiag_.FixIts[i].Replacement
         << "\",\"description\":\"" << LastDiag_.FixIts[i].Description << "\"}";
    }
    OS << "]";
  }
  OS << "\n}\n";
}

void DetailedDiagEmitter::emitText(ir::raw_ostream& OS) const {
  if (!HasLast_) return;
  // 文本格式：<level>: <message> [code=<code>]
  OS << getDiagnosticLevelName(LastDiag_.Level) << ": " << LastDiag_.Message;
  OS << " [code=0x" << std::hex << static_cast<uint32_t>(LastDiag_.Code) << std::dec << "]";
  if (!LastDiag_.Category.empty()) {
    OS << " [" << LastDiag_.Category << "]";
  }
  if (!LastDiag_.FlagName.empty()) {
    OS << " [" << LastDiag_.FlagName << "]";
  }
  OS << "\n";
}

} // namespace diag
} // namespace blocktype
```

---

## 3. 产出文件列表

| 操作 | 文件路径 |
|------|----------|
| 新增 | `include/blocktype/Frontend/StructuredDiagnostic.h` |
| 新增 | `src/Frontend/StructuredDiagnostic.cpp` |
| 新增 | `tests/unit/Frontend/StructuredDiagnosticTest.cpp` |

---

## 4. CMakeLists.txt 修改

### `src/Frontend/CMakeLists.txt`

在 `add_library(blocktypeFrontend ...)` 的源文件列表中添加：

```
StructuredDiagnostic.cpp
```

### `tests/unit/Frontend/CMakeLists.txt`

追加以下内容：

```cmake
# StructuredDiagnostic unit test (GTest)

add_executable(StructuredDiagnosticTest
  StructuredDiagnosticTest.cpp
)

target_link_libraries(StructuredDiagnosticTest
  PRIVATE
    blocktypeFrontend
    blocktype-basic
    blocktype-ir
    ${LLVM_LIBS}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(StructuredDiagnosticTest
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(StructuredDiagnosticTest)
```

---

## 5. GTest 测试用例

### `tests/unit/Frontend/StructuredDiagnosticTest.cpp`

```cpp
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "blocktype/Frontend/StructuredDiagnostic.h"

using namespace blocktype;
using namespace blocktype::diag;

// ============================================================
// 测试：DetailedStructuredDiagnostic 创建和字段赋值
// ============================================================

TEST(StructuredDiagnosticTest, CreateDetailedDiag) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Message = "Cannot map QualType to IRType";
  D.Category = "type-error";
  D.FlagName = "-Wir";
  D.IRRelatedDialect = ir::dialect::DialectID::Cpp;

  EXPECT_EQ(D.Level, DiagnosticLevel::Error);
  EXPECT_EQ(D.Message, "Cannot map QualType to IRType");
  EXPECT_EQ(D.Category, "type-error");
  EXPECT_EQ(D.FlagName, "-Wir");
  EXPECT_TRUE(D.IRRelatedDialect.has_value());
  EXPECT_EQ(*D.IRRelatedDialect, ir::dialect::DialectID::Cpp);
  EXPECT_FALSE(D.IRRelatedOpcode.has_value());
}

// ============================================================
// 测试：FixItHint
// ============================================================

TEST(StructuredDiagnosticTest, FixItHint) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Warning;
  D.Message = "Use int instead";

  FixItHint FI;
  FI.Replacement = "int";
  FI.Description = "Replace with int";
  D.FixIts.push_back(FI);

  ASSERT_EQ(D.FixIts.size(), 1u);
  EXPECT_EQ(D.FixIts[0].Replacement, "int");
  EXPECT_EQ(D.FixIts[0].Description, "Replace with int");
}

// ============================================================
// 测试：DiagnosticNote
// ============================================================

TEST(StructuredDiagnosticTest, DiagnosticNote) {
  DetailedStructuredDiagnostic D;
  DiagnosticNote N;
  N.Message = "See declaration here";
  D.DetailedNotes.push_back(N);

  ASSERT_EQ(D.DetailedNotes.size(), 1u);
  EXPECT_EQ(D.DetailedNotes[0].Message, "See declaration here");
}

// ============================================================
// 测试：DetailedDiagEmitter emit + JSON 输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitJSON) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Code = DiagnosticCode::TypeMappingFailed;
  D.Group = DiagnosticGroup::TypeMapping;
  D.Message = "Cannot map QualType to IRType";
  D.Category = "type-error";
  D.FlagName = "-Wir";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string JSON;
  ir::raw_string_ostream OS(JSON);
  Emitter.emitJSON(OS);

  EXPECT_NE(JSON.find("type-error"), std::string::npos);
  EXPECT_NE(JSON.find("Cannot map QualType to IRType"), std::string::npos);
  EXPECT_NE(JSON.find("\"flag\": \"-Wir\""), std::string::npos);
}

// ============================================================
// 测试：SARIF 输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitSARIF) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Error;
  D.Code = DiagnosticCode::TypeMappingFailed;
  D.Message = "test error";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string SARIF;
  ir::raw_string_ostream OS(SARIF);
  Emitter.emitSARIF(OS);

  EXPECT_NE(SARIF.find("sarif-schema"), std::string::npos);
  EXPECT_NE(SARIF.find("test error"), std::string::npos);
}

// ============================================================
// 测试：文本输出
// ============================================================

TEST(StructuredDiagnosticTest, EmitText) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Warning;
  D.Code = DiagnosticCode::TypeMappingAmbiguous;
  D.Message = "ambiguous type";
  D.Category = "type-warn";
  D.FlagName = "-Wtype";

  DetailedDiagEmitter Emitter;
  Emitter.emit(D);

  std::string Text;
  ir::raw_string_ostream OS(Text);
  Emitter.emitText(OS);

  EXPECT_NE(Text.find("ambiguous type"), std::string::npos);
  EXPECT_NE(Text.find("type-warn"), std::string::npos);
  EXPECT_NE(Text.find("-Wtype"), std::string::npos);
}

// ============================================================
// 测试：基类 StructuredDiagnostic 向下兼容
// ============================================================

TEST(StructuredDiagnosticTest, BaseClassCompatibility) {
  DetailedStructuredDiagnostic D;
  D.Level = DiagnosticLevel::Fatal;
  D.Code = DiagnosticCode::VerificationFailed;
  D.Group = DiagnosticGroup::IRVerification;
  D.Message = "fatal verification failure";

  // 基类方法仍然可用
  EXPECT_EQ(D.getLevel(), DiagnosticLevel::Fatal);
  EXPECT_EQ(D.getCode(), DiagnosticCode::VerificationFailed);
  EXPECT_EQ(D.getGroup(), DiagnosticGroup::IRVerification);
}

// ============================================================
// 测试：DiagnosticLevel 枚举值
// ============================================================

TEST(StructuredDiagnosticTest, DiagnosticLevelValues) {
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Note), 0);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Remark), 1);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Warning), 2);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Error), 3);
  EXPECT_EQ(static_cast<uint8_t>(DiagnosticLevel::Fatal), 4);
}
```

---

## 6. 验收标准

1. `DetailedStructuredDiagnostic` 继承自 `StructuredDiagnostic`，保持基类字段不变
2. 新增字段 `PrimaryLoc`（`blocktype::SourceLocation`）、`Ranges`、`RelatedLocs`、`Category`、`FlagName`、`FixIts`、`DetailedNotes`、`IRRelatedDialect`、`IRRelatedOpcode` 全部可赋值和读取
3. `FixItHint` 包含 `Range`/`Replacement`/`Description`
4. `DiagnosticNote` 包含 `Loc`/`Message`
5. `DetailedDiagEmitter` 继承自 `StructuredDiagEmitter`，实现 `emit()`、`emitSARIF()`、`emitJSON()`、`emitText()`
6. 不修改 `IR/IRDiagnostic.h` 中的现有 `StructuredDiagnostic`、`StructuredDiagEmitter`、`DiagnosticGroupManager`
7. `IRRelatedDialect` 使用 `ir::dialect::DialectID`（非 `ir::DialectID`）
8. `IRRelatedOpcode` 使用 `ir::Opcode`
9. 所有 GTest 测试通过
