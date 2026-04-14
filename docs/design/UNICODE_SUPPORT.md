# BlockType Unicode 支持设计

## 概述

BlockType 编译器完全支持 Unicode 标识符，遵循 Unicode 标准附件 #31 (UAX #31) 的规范。本文档详细描述 Unicode 支持的实现细节。

## UAX #31 标准

### 标识符规则

UAX #31 定义了标识符的语法规则：

#### ID_Start（标识符起始字符）

标识符可以以下列字符开头：

- **Lu**: Letter, uppercase（大写字母）
- **Ll**: Letter, lowercase（小写字母）
- **Lt**: Letter, titlecase（标题字母）
- **Lm**: Letter, modifier（修饰字母）
- **Lo**: Letter, other（其他字母，包括 CJK 字符）
- **Nl**: Number, letter（字母数字）
- **Other_ID_Start**: 明确列出的其他字符

#### ID_Continue（标识符后续字符）

标识符可以包含以下字符：

- **ID_Start**: 所有起始字符
- **Mn**: Mark, nonspacing（非间距标记）
- **Mc**: Mark, spacing combining（间距组合标记）
- **Nd**: Number, decimal digit（十进制数字）
- **Pc**: Punctuation, connector（连接标点，如下划线）
- **Other_ID_Continue**: 明确列出的其他字符

### Pattern_Syntax 和 Pattern_White_Space

UAX #31 还定义了不应出现在标识符中的字符：

- **Pattern_Syntax**: 用于语法的标点符号
- **Pattern_White_Space**: 空白字符

## 实现设计

### 1. Unicode 工具类

```cpp
// include/blocktype/Basic/Unicode.h
#pragma once

#include "llvm/ADT/StringRef.h"
#include <cstdint>
#include <string>

namespace blocktype {

class Unicode {
public:
  /// 判断字符是否可以作为标识符起始字符（UAX #31 ID_Start）
  static bool isIDStart(uint32_t CP);
  
  /// 判断字符是否可以作为标识符后续字符（UAX #31 ID_Continue）
  static bool isIDContinue(uint32_t CP);
  
  /// 判断是否是 CJK 字符
  static bool isCJK(uint32_t CP);
  
  /// 判断是否是中文字符
  static bool isChinese(uint32_t CP);
  
  /// NFC 规范化
  static std::string normalizeNFC(llvm::StringRef Input);
  
  /// 获取 UTF-8 字符的字节长度
  static unsigned getUTF8ByteLength(uint8_t FirstByte);
  
  /// 解码 UTF-8 字符
  static uint32_t decodeUTF8(const char *&Ptr, const char *End);
  
  /// 编码 UTF-8 字符
  static void encodeUTF8(uint32_t CP, char *&Ptr);
};

} // namespace blocktype
```

### 2. ID_Start 和 ID_Continue 判断

```cpp
// src/Basic/Unicode.cpp
#include "blocktype/Basic/Unicode.h"
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/normlzr.h>

namespace blocktype {

bool Unicode::isIDStart(uint32_t CP) {
  // UAX #31 ID_Start
  int32_t GC = u_charType(CP);
  
  // 检查基本类别
  if (GC == U_UPPERCASE_LETTER ||      // Lu
      GC == U_LOWERCASE_LETTER ||      // Ll
      GC == U_TITLECASE_LETTER ||      // Lt
      GC == U_MODIFIER_LETTER ||       // Lm
      GC == U_OTHER_LETTER ||          // Lo (包括 CJK)
      GC == U_LETTER_NUMBER) {         // Nl
    return true;
  }
  
  // 检查 Other_ID_Start 属性
  if (u_hasBinaryProperty(CP, UCHAR_ID_START)) {
    return true;
  }
  
  // ASCII 快速路径
  if (CP < 128) {
    return (CP >= 'A' && CP <= 'Z') ||
           (CP >= 'a' && CP <= 'z') ||
           CP == '_';
  }
  
  return false;
}

bool Unicode::isIDContinue(uint32_t CP) {
  // ID_Continue = ID_Start + 特定类别
  
  if (isIDStart(CP)) {
    return true;
  }
  
  // 检查其他类别
  int32_t GC = u_charType(CP);
  if (GC == U_NON_SPACING_MARK ||      // Mn
      GC == U_COMBINING_SPACING_MARK ||// Mc
      GC == U_DECIMAL_DIGIT_NUMBER ||  // Nd
      GC == U_CONNECTOR_PUNCTUATION) { // Pc
    return true;
  }
  
  // 检查 Other_ID_Continue 属性
  if (u_hasBinaryProperty(CP, UCHAR_ID_CONTINUE)) {
    return true;
  }
  
  // ASCII 快速路径
  if (CP < 128) {
    return (CP >= '0' && CP <= '9');
  }
  
  return false;
}

bool Unicode::isCJK(uint32_t CP) {
  // CJK Unified Ideographs
  return (CP >= 0x4E00 && CP <= 0x9FFF) ||   // CJK Unified Ideographs
         (CP >= 0x3400 && CP <= 0x4DBF) ||   // CJK Extension A
         (CP >= 0x20000 && CP <= 0x2A6DF) || // CJK Extension B
         (CP >= 0x2A700 && CP <= 0x2B73F) || // CJK Extension C
         (CP >= 0x2B740 && CP <= 0x2B81F) || // CJK Extension D
         (CP >= 0x2B820 && CP <= 0x2CEAF);   // CJK Extension E
}

bool Unicode::isChinese(uint32_t CP) {
  return isCJK(CP);
}

} // namespace blocktype
```

### 3. NFC 规范化

NFC（Normalization Form C）是 Unicode 规范化的一种形式，确保语义相同的字符串有相同的二进制表示。

```cpp
std::string Unicode::normalizeNFC(llvm::StringRef Input) {
  UErrorCode Status = U_ZERO_ERROR;
  
  // 将 UTF-8 输入转换为 ICU UnicodeString
  icu::UnicodeString Source = icu::UnicodeString::fromUTF8(
    icu::StringPiece(Input.data(), Input.size()));
  
  // 执行 NFC 规范化
  icu::UnicodeString Result;
  icu::Normalizer::normalize(Source, UNORM_NFC, Status);
  
  if (U_SUCCESS(Status)) {
    // 转换回 UTF-8
    std::string Output;
    Result.toUTF8String(Output);
    return Output;
  }
  
  // 规范化失败，返回原始输入
  return Input.str();
}
```

**为什么需要 NFC 规范化？**

```cpp
// 示例：字符 "é" 有两种表示方式
const char* s1 = "é";                    // 单个字符 U+00E9
const char* s2 = "é";                    // 'e' + 组合重音 U+0301

// 不进行规范化，这两个字符串不相等
// 进行 NFC 规范化后，都变成 "é" (U+00E9)
```

### 4. UTF-8 编解码

```cpp
unsigned Unicode::getUTF8ByteLength(uint8_t FirstByte) {
  // UTF-8 编码规则：
  // 0xxxxxxx: 1 字节 (ASCII)
  // 110xxxxx: 2 字节
  // 1110xxxx: 3 字节
  // 11110xxx: 4 字节
  
  if ((FirstByte & 0x80) == 0) return 1;
  if ((FirstByte & 0xE0) == 0xC0) return 2;
  if ((FirstByte & 0xF0) == 0xE0) return 3;
  if ((FirstByte & 0xF8) == 0xF0) return 4;
  
  return 1; // 无效字节，当作 1 字节处理
}

uint32_t Unicode::decodeUTF8(const char *&Ptr, const char *End) {
  if (Ptr >= End) return 0;
  
  uint8_t FirstByte = static_cast<uint8_t>(*Ptr++);
  
  // 1 字节序列 (ASCII)
  if ((FirstByte & 0x80) == 0) {
    return FirstByte;
  }
  
  // 2 字节序列
  if ((FirstByte & 0xE0) == 0xC0) {
    if (Ptr >= End) return FirstByte;
    uint8_t Byte2 = static_cast<uint8_t>(*Ptr++);
    return ((FirstByte & 0x1F) << 6) | (Byte2 & 0x3F);
  }
  
  // 3 字节序列
  if ((FirstByte & 0xF0) == 0xE0) {
    if (Ptr + 1 >= End) return FirstByte;
    uint8_t Byte2 = static_cast<uint8_t>(*Ptr++);
    uint8_t Byte3 = static_cast<uint8_t>(*Ptr++);
    return ((FirstByte & 0x0F) << 12) | 
           ((Byte2 & 0x3F) << 6) | 
           (Byte3 & 0x3F);
  }
  
  // 4 字节序列
  if ((FirstByte & 0xF8) == 0xF0) {
    if (Ptr + 2 >= End) return FirstByte;
    uint8_t Byte2 = static_cast<uint8_t>(*Ptr++);
    uint8_t Byte3 = static_cast<uint8_t>(*Ptr++);
    uint8_t Byte4 = static_cast<uint8_t>(*Ptr++);
    return ((FirstByte & 0x07) << 18) | 
           ((Byte2 & 0x3F) << 12) | 
           ((Byte3 & 0x3F) << 6) | 
           (Byte4 & 0x3F);
  }
  
  return FirstByte; // 无效序列
}

void Unicode::encodeUTF8(uint32_t CP, char *&Ptr) {
  // 1 字节序列 (ASCII)
  if (CP < 0x80) {
    *Ptr++ = static_cast<char>(CP);
    return;
  }
  
  // 2 字节序列
  if (CP < 0x800) {
    *Ptr++ = static_cast<char>(0xC0 | (CP >> 6));
    *Ptr++ = static_cast<char>(0x80 | (CP & 0x3F));
    return;
  }
  
  // 3 字节序列
  if (CP < 0x10000) {
    *Ptr++ = static_cast<char>(0xE0 | (CP >> 12));
    *Ptr++ = static_cast<char>(0x80 | ((CP >> 6) & 0x3F));
    *Ptr++ = static_cast<char>(0x80 | (CP & 0x3F));
    return;
  }
  
  // 4 字节序列
  *Ptr++ = static_cast<char>(0xF0 | (CP >> 18));
  *Ptr++ = static_cast<char>(0x80 | ((CP >> 12) & 0x3F));
  *Ptr++ = static_cast<char>(0x80 | ((CP >> 6) & 0x3F));
  *Ptr++ = static_cast<char>(0x80 | (CP & 0x3F));
}
```

## 源文件编码检测

### 支持的编码格式

- UTF-8（推荐，默认）
- UTF-8 with BOM
- UTF-16 LE/BE
- UTF-32 LE/BE

### 检测算法

```cpp
enum class SourceEncoding {
  UTF8,
  UTF8_BOM,
  UTF16_LE,
  UTF16_BE,
  UTF32_LE,
  UTF32_BE,
  Unknown
};

SourceEncoding detectEncoding(const char *Data, size_t Length) {
  // 检查 BOM (Byte Order Mark)
  if (Length >= 3 && 
      static_cast<uint8_t>(Data[0]) == 0xEF &&
      static_cast<uint8_t>(Data[1]) == 0xBB &&
      static_cast<uint8_t>(Data[2]) == 0xBF) {
    return SourceEncoding::UTF8_BOM;
  }
  
  if (Length >= 2) {
    // UTF-16 LE BOM: FF FE
    if (static_cast<uint8_t>(Data[0]) == 0xFF &&
        static_cast<uint8_t>(Data[1]) == 0xFE) {
      return Length >= 4 && 
             static_cast<uint8_t>(Data[2]) == 0x00 &&
             static_cast<uint8_t>(Data[3]) == 0x00
        ? SourceEncoding::UTF32_LE
        : SourceEncoding::UTF16_LE;
    }
    
    // UTF-16 BE BOM: FE FF
    if (static_cast<uint8_t>(Data[0]) == 0xFE &&
        static_cast<uint8_t>(Data[1]) == 0xFF) {
      return SourceEncoding::UTF16_BE;
    }
    
    // UTF-32 BE BOM: 00 00 FE FF
    if (Length >= 4 &&
        static_cast<uint8_t>(Data[0]) == 0x00 &&
        static_cast<uint8_t>(Data[1]) == 0x00 &&
        static_cast<uint8_t>(Data[2]) == 0xFE &&
        static_cast<uint8_t>(Data[3]) == 0xFF) {
      return SourceEncoding::UTF32_BE;
    }
  }
  
  // 默认为 UTF-8（无 BOM）
  return SourceEncoding::UTF8;
}
```

## 字符串字面量处理

### 字符串字面量类型

```cpp
// 英文字符串
const char* msg1 = "Hello, World!";

// 中文字符串（UTF-8）
const char* msg2 = "你好，世界！";

// 原始字符串（支持多行和中文）
const char* msg3 = R"(这是一个
多行字符串
包含中文)";

// Unicode 转义
const char* msg4 = "\u4F60\u597D";  // "你好"

// 宽字符串（UTF-16 或 UTF-32，取决于平台）
const wchar_t* msg5 = L"你好";

// UTF-16 字符串
const char16_t* msg6 = u"你好";

// UTF-32 字符串
const char32_t* msg7 = U"你好";

// UTF-8 字符串（C++20）
const char8_t* msg8 = u8"你好";
```

### 字符串字面量编码

编译器内部统一使用 UTF-8 编码处理字符串字面量：

1. 读取源文件时，转换为 UTF-8
2. 解析字符串字面量时，保持 UTF-8 编码
3. 生成目标码时，使用 UTF-8 编码

## 性能优化

### 1. 快速路径

对于 ASCII 字符，使用快速路径：

```cpp
inline bool isIDStart(uint32_t CP) {
  // ASCII 快速路径
  if (CP < 128) {
    return (CP >= 'A' && CP <= 'Z') ||
           (CP >= 'a' && CP <= 'z') ||
           CP == '_';
  }
  
  // Unicode 慢速路径
  return isIDStartUnicode(CP);
}
```

### 2. 缓存

缓存 NFC 规范化结果：

```cpp
class NFCNormalizer {
  llvm::StringMap<std::string> Cache;
  
public:
  StringRef normalize(StringRef Input) {
    auto It = Cache.find(Input);
    if (It != Cache.end()) {
      return It->second;
    }
    
    std::string Normalized = doNormalize(Input);
    Cache[Input] = Normalized;
    return Cache[Input];
  }
};
```

### 3. 延迟规范化

仅在需要时进行 NFC 规范化：

```cpp
class IdentifierInfo {
  std::string Name;
  bool Normalized = false;
  
public:
  StringRef getName() const { return Name; }
  
  StringRef getNormalizedName() {
    if (!Normalized) {
      Name = Unicode::normalizeNFC(Name);
      Normalized = true;
    }
    return Name;
  }
};
```

## 测试用例

### 1. Unicode 标识符测试

```cpp
TEST(UnicodeTest, ChineseIdentifiers) {
  EXPECT_TRUE(Unicode::isIDStart(U'中'));
  EXPECT_TRUE(Unicode::isIDStart(U'文'));
  EXPECT_TRUE(Unicode::isIDContinue(U'文'));
  EXPECT_TRUE(Unicode::isChinese(U'中'));
  EXPECT_TRUE(Unicode::isCJK(U'文'));
}

TEST(UnicodeTest, MixedIdentifiers) {
  EXPECT_TRUE(Unicode::isIDStart(U'A'));
  EXPECT_TRUE(Unicode::isIDStart(U'_'));
  EXPECT_TRUE(Unicode::isIDContinue(U'0'));
  EXPECT_TRUE(Unicode::isIDContinue(U'中'));
}
```

### 2. NFC 规范化测试

```cpp
TEST(NFCTest, Normalize) {
  // 字符 "é" 的两种表示方式
  std::string s1 = "é";                    // U+00E9
  std::string s2 = "é";                    // 'e' + U+0301
  
  EXPECT_EQ(Unicode::normalizeNFC(s1), Unicode::normalizeNFC(s2));
}
```

### 3. UTF-8 编解码测试

```cpp
TEST(UTF8Test, EncodeDecode) {
  const char* Input = "你好";
  const char* Ptr = Input;
  const char* End = Input + strlen(Input);
  
  uint32_t CP1 = Unicode::decodeUTF8(Ptr, End);
  EXPECT_EQ(CP1, U'你');
  
  uint32_t CP2 = Unicode::decodeUTF8(Ptr, End);
  EXPECT_EQ(CP2, U'好');
}
```

## 依赖

- **ICU 库**：International Components for Unicode
  - 提供 Unicode 字符属性查询
  - 提供规范化功能
  - 版本要求：ICU 60+

## 总结

BlockType 的 Unicode 支持完全遵循 UAX #31 标准，确保：

1. **标准兼容**：使用国际标准，与其他编译器兼容
2. **性能优化**：通过快速路径和缓存提高性能
3. **正确性**：使用成熟的 ICU 库确保正确性
4. **易用性**：支持中文标识符，降低编程门槛
