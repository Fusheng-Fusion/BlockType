# Task A-F13：IR 层头文件依赖图 + CMake 子库拆分 — 优化版

## 红线 Checklist 确认

| # | 红线 | 状态 | 说明 |
|---|------|------|------|
| 1 | 架构优先 | ✅ | CMake 子库拆分保持 IR 核心库无 LLVM 依赖，依赖关系清晰 |
| 2 | 多前端多后端自由组合 | ✅ | 子库独立编译链接，前端/后端可按需组合 |
| 3 | 渐进式改造 | ✅ | 仅修改 CMakeLists + 新增 2 个子目录 CMakeLists，不修改源代码 |
| 4 | 现有功能不退化 | ✅ | 全量测试必须通过，无新增失败 |
| 5 | 接口抽象优先 | ✅ | 子库间通过抽象接口（CacheStorage）交互 |
| 6 | IR 中间层解耦 | ✅ | 依赖图无循环，核心库不依赖子库 |

## 依赖
A.1 ~ A.7 全部完成 ✅、A-F1 ~ A-F12 全部完成 ✅

## 产出文件

| 操作 | 文件路径 |
|------|----------|
| 修改 | `src/IR/CMakeLists.txt`（拆分：移出 Cache/Telemetry 文件） |
| 新增 | `src/IR/Cache/CMakeLists.txt` |
| 新增 | `src/IR/Telemetry/CMakeLists.txt` |
| 移动 | `src/IR/IRCache.cpp` → `src/IR/Cache/IRCache.cpp` |
| 移动 | `src/IR/IRTelemetry.cpp` → `src/IR/Telemetry/IRTelemetry.cpp` |

## 当前文件清单分析

### 当前 `src/IR/CMakeLists.txt` 中的 31 个源文件

| 文件 | 归属子库 | 说明 |
|------|----------|------|
| `APInt.cpp` | blocktype-ir | ADT 基础 |
| `IRBasicBlock.cpp` | blocktype-ir | 核心类型 |
| `IRBuilder.cpp` | blocktype-ir | 核心类型 |
| `IRConstant.cpp` | blocktype-ir | 核心类型 |
| `IRContext.cpp` | blocktype-ir | 核心类型 |
| `IRFormatVersion.cpp` | blocktype-ir | 核心类型（Cache/Telemetry 也依赖，但 IR 核心库持有） |
| `IRFunction.cpp` | blocktype-ir | 核心类型 |
| `IRInstruction.cpp` | blocktype-ir | 核心类型 |
| `IRModule.cpp` | blocktype-ir | 核心类型 |
| `IRPass.cpp` | blocktype-ir | 核心类型 |
| `IRSerializer.cpp` | blocktype-ir | 核心类型 |
| `IRType.cpp` | blocktype-ir | 核心类型 |
| `IRTypeContext.cpp` | blocktype-ir | 核心类型 |
| `IRValue.cpp` | blocktype-ir | 核心类型 |
| `IRVerifier.cpp` | blocktype-ir | 核心类型 |
| `BackendCapability.cpp` | blocktype-ir | 核心类型 |
| `DialectLoweringPass.cpp` | blocktype-ir | 核心类型 |
| `IRDialect.cpp` | blocktype-ir | 核心类型 |
| `IRDebugMetadata.cpp` | blocktype-ir | 核心类型 |
| `IRDebugInfo.cpp` | blocktype-ir | 核心类型 |
| `TargetLayout.cpp` | blocktype-ir | 核心类型 |
| `raw_ostream.cpp` | blocktype-ir | 核心类型 |
| `IRDiagnostic.cpp` | blocktype-ir | 核心类型 |
| `IRIntegrity.cpp` | blocktype-ir | 核心类型 |
| `IRConversionResult.cpp` | blocktype-ir | 核心类型 |
| `IRErrorCode.cpp` | blocktype-ir | 核心类型 |
| `IRPasses.cpp` | blocktype-ir | A-F11 新增 |
| `IRContract.cpp` | blocktype-ir | A-F11 新增 |
| `IRFFI.cpp` | blocktype-ir | A-F12 新增 |
| **`IRCache.cpp`** | **blocktype-ir-cache** | 缓存子库，需移出 |
| **`IRTelemetry.cpp`** | **blocktype-ir-telemetry** | 遥测子库，需移出 |

### IR 层头文件依赖图（无循环验证）

```
基础层（无 IR 依赖）：
  ADT.h → (blocktype-basic: DenseMap/SmallVector/StringRef/BumpPtrAllocator等)
  TargetLayout.h → ADT.h
  IRType.h → ADT.h, TargetLayout.h
  IRFormatVersion.h → (无依赖，仅 stdint/string)
  IRDebugMetadata.h → IRType.h (前向声明)

值层：
  IRValue.h → IRType.h, ValueKind/Opcode/LinkageKind枚举
  IRInstruction.h → IRValue.h, IRDebugInfo.h
  IRConstant.h → IRValue.h, APInt/APFloat

函数/模块层：
  IRBasicBlock.h → IRInstruction.h
  IRFunction.h → IRBasicBlock.h, IRFunctionType
  IRModule.h → IRFunction.h, IRTypeContext.h, IRConstant.h

工具层：
  IRTypeContext.h → IRType.h, DenseMap/FoldingSet/StringMap
  IRContext.h → IRTypeContext.h, BumpPtrAllocator
  IRBuilder.h → IRContext.h, IRInstruction.h, IRConstant.h, IRBasicBlock.h
  IRPass.h → IRModule.h (前向声明)
  IRVerifier.h → IRPass.h, IRModule.h

扩展层：
  IRDialect.h → DialectID枚举
  DialectLoweringPass.h → IRPass.h, IRDialect.h
  BackendCapability.h → (轻量)
  IRDiagnostic.h → ADT.h
  IRIntegrity.h → IRModule.h (前向声明)
  IRConversionResult.h → ADT.h
  IRErrorCode.h → (轻量)
  IRPasses.h → IRPass.h
  IRContract.h → ADT.h
  IRFFI.h → ADT.h

独立子库（无 IR 核心依赖）：
  IRTelemetry.h → ADT.h (仅使用 SmallVector/StringRef)
  
独立子库（依赖 IR 核心前向声明）：
  IRCache.h → ADT.h, IRFormatVersion.h, IRModule/IRTypeContext 前向声明
```

**结论**：依赖图无循环。

---

## Part 1：修改后的 `src/IR/CMakeLists.txt`

**完整替换内容**：

```cmake
# libblocktype-ir — IR 核心库（无 LLVM 依赖）
add_library(blocktype-ir
  APInt.cpp
  IRBasicBlock.cpp
  IRBuilder.cpp
  IRConstant.cpp
  IRContext.cpp
  IRFormatVersion.cpp
  IRFunction.cpp
  IRInstruction.cpp
  IRModule.cpp
  IRPass.cpp
  IRSerializer.cpp
  IRType.cpp
  IRTypeContext.cpp
  IRValue.cpp
  IRVerifier.cpp
  BackendCapability.cpp
  DialectLoweringPass.cpp
  IRDialect.cpp
  IRDebugMetadata.cpp
  IRDebugInfo.cpp
  TargetLayout.cpp
  raw_ostream.cpp
  IRDiagnostic.cpp
  IRIntegrity.cpp
  IRConversionResult.cpp
  IRErrorCode.cpp
  IRPasses.cpp
  IRContract.cpp
  IRFFI.cpp
)

target_include_directories(blocktype-ir
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir
  PUBLIC
    blocktype-basic
)

install(TARGETS blocktype-ir
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/blocktype/IR
  DESTINATION include/blocktype
  FILES_MATCHING PATTERN "*.h"
)

# 子库
add_subdirectory(Cache)
add_subdirectory(Telemetry)
```

---

## Part 2：新增 `src/IR/Cache/CMakeLists.txt`

```cmake
# libblocktype-ir-cache — 编译缓存子库
add_library(blocktype-ir-cache
  IRCache.cpp
)

target_include_directories(blocktype-ir-cache
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir-cache
  PUBLIC
    blocktype-basic
    blocktype-ir
)

install(TARGETS blocktype-ir-cache
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
```

---

## Part 3：新增 `src/IR/Telemetry/CMakeLists.txt`

```cmake
# libblocktype-ir-telemetry — 遥测子库
add_library(blocktype-ir-telemetry
  IRTelemetry.cpp
)

target_include_directories(blocktype-ir-telemetry
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir-telemetry
  PUBLIC
    blocktype-basic
)

install(TARGETS blocktype-ir-telemetry
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
```

---

## Part 4：文件移动

需要将 2 个源文件移动到对应子目录：

```bash
# 创建子目录
mkdir -p src/IR/Cache
mkdir -p src/IR/Telemetry

# 移动 Cache 文件
mv src/IR/IRCache.cpp src/IR/Cache/IRCache.cpp

# 移动 Telemetry 文件
mv src/IR/IRTelemetry.cpp src/IR/Telemetry/IRTelemetry.cpp
```

头文件（`include/blocktype/IR/IRCache.h` 和 `IRTelemetry.h`）**不移动**，保持在原位置。
原因：保持 `#include "blocktype/IR/IRCache.h"` 路径不变，避免破坏已有代码。

---

## Part 5：测试链接更新

`tests/unit/IR/CMakeLists.txt` 需要更新链接库：

```cmake
add_executable(blocktype-ir-test
  IRBackendCapabilityTest.cpp
  IRBuilderTest.cpp
  IRDialectTest.cpp
  IRSerializerTest.cpp
  IRTelemetryTest.cpp
  IRDebugInfoTest.cpp
  IRVerifierTest.cpp
  IRDiagnosticTest.cpp
  IRCacheTest.cpp
  IRIntegrityTest.cpp
  IRConversionResultTest.cpp
  IRErrorCodeTest.cpp
  IRPassesTest.cpp
  IRContractTest.cpp
  IRFFITest.cpp
)

target_link_libraries(blocktype-ir-test
  PRIVATE
    blocktype-ir
    blocktype-ir-cache
    blocktype-ir-telemetry
    gtest
    gtest_main
)

add_test(NAME IRBuilderTest COMMAND blocktype-ir-test)
```

---

## Part 6：验收标准

### V1: libblocktype-ir 无 LLVM 符号

```bash
# 构建产物路径取决于构建系统
nm libblocktype-ir.a 2>/dev/null | grep "llvm::" | wc -l
# 输出 == 0
```

### V2: 子库独立编译

```bash
cmake --build build --target blocktype-ir
cmake --build build --target blocktype-ir-cache
cmake --build build --target blocktype-ir-telemetry
# 全部退出码 == 0
```

### V3: 全量测试通过

```bash
cd /Users/yuan/Documents/BlockType/build && ctest --output-on-failure
# 全部通过，无新增失败
```

### V4: 头文件依赖无循环

通过对每个头文件单独 `#include` 编译通过来验证。

---

## Part 7：dev-tester 执行步骤

1. **创建子目录**：
   ```bash
   mkdir -p /Users/yuan/Documents/BlockType/src/IR/Cache
   mkdir -p /Users/yuan/Documents/BlockType/src/IR/Telemetry
   ```

2. **移动源文件**：
   ```bash
   mv /Users/yuan/Documents/BlockType/src/IR/IRCache.cpp /Users/yuan/Documents/BlockType/src/IR/Cache/IRCache.cpp
   mv /Users/yuan/Documents/BlockType/src/IR/IRTelemetry.cpp /Users/yuan/Documents/BlockType/src/IR/Telemetry/IRTelemetry.cpp
   ```

3. **修改 `src/IR/CMakeLists.txt`**：用 Part 1 的内容替换（移除 IRCache.cpp 和 IRTelemetry.cpp，末尾添加 add_subdirectory）

4. **创建 `src/IR/Cache/CMakeLists.txt`**：复制 Part 2 内容

5. **创建 `src/IR/Telemetry/CMakeLists.txt`**：复制 Part 3 内容

6. **修改 `tests/unit/IR/CMakeLists.txt`**：在 `target_link_libraries` 中添加 `blocktype-ir-cache` 和 `blocktype-ir-telemetry`（Part 5）

7. **重新配置并构建**：
   ```bash
   cd /Users/yuan/Documents/BlockType/build && cmake .. && cmake --build .
   ```

8. **运行全量测试**：
   ```bash
   ctest --output-on-failure
   ```

9. **验证子库独立编译**：
   ```bash
   cmake --build . --target blocktype-ir
   cmake --build . --target blocktype-ir-cache
   cmake --build . --target blocktype-ir-telemetry
   ```

10. **验证无 LLVM 符号**：
    ```bash
    nm lib/libblocktype-ir.a 2>/dev/null | grep "llvm::" | wc -l
    # 期望输出 0
    ```

### ⚠️ 注意事项
- **不移动头文件**：`include/blocktype/IR/IRCache.h` 和 `IRTelemetry.h` 保持原位
- **IRCache.cpp 内部 include 路径不变**：因为它 `#include "blocktype/IR/IRCache.h"`，由 `target_include_directories` 保证路径
- **IRTelemetry.h 的命名空间**是 `blocktype::telemetry`（不是 `blocktype::ir::telemetry`），但它 include 了 `blocktype/IR/ADT.h`，所以依赖 `blocktype-basic` 即可
- **IRCache.h** 前向声明了 `blocktype::ir::IRModule` 和 `IRTypeContext`，`CompilationCacheManager` 方法参数使用了这些类型，所以 `blocktype-ir-cache` 需要链接 `blocktype-ir`
- 如果测试中 `IRCacheTest.cpp` 或 `IRTelemetryTest.cpp` 有额外的链接需求，dev-tester 需要根据编译错误调整
