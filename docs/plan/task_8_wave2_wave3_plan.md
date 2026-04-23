# Phase 8 Wave 2/3 详细开发计划

> **规划人：** planner | **日期：** 2026-04-24
> **前置状态：** Wave 1 已完成（8.1.1 + 8.1.2，910/910 测试通过）
> **当前并行：** Task 7.3.2 Contract 语义正在开发中（与 Phase 8 无直接依赖）

---

## 一、Wave 1 完成确认

| Task | 状态 | 关键交付物 |
|------|------|-----------|
| 8.1.1 | ✅ | `X86_64TargetInfo` + `AArch64TargetInfo` + `TargetInfo::Create()` 工厂 |
| 8.1.2 | ✅ | `X86_64ABIInfo` + `AArch64ABIInfo` + `Expand`/`Coerce` + `sret`/`inreg` 属性 |

---

## 二、Wave 2 详细计划

### Task 8.1.3 — 名称修饰增强（9h）

**目标：** 添加 Itanium ABI substitution 压缩，增强 mangled name 兼容性

**当前差距：**
- 无 substitution 表（`S_`/`S0_` 回指编码）
- 模板类型参数用 vendor extension（`u5tparam`）而非标准编码
- 无标准实体预定义（`St`=std::, `Sa`=std::allocator）
- `Mangler` 类无 substitution 状态管理

#### 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/CodeGen/Mangler.h` | 修改 | 添加 Substitutions 表 + 方法声明 |
| `src/CodeGen/Mangler.cpp` | 修改 | 实现 substitution 压缩逻辑 |
| `tests/unit/CodeGen/ManglerTest.cpp` | 修改 | 添加 substitution 测试用例 |

#### 详细开发步骤

**步骤 1：Substitution 表基础设施（3h）**

在 `Mangler.h` 的 private 区域添加：

```cpp
//===------------------------------------------------------------------===//
// Substitution 压缩（Itanium ABI §5.3.5）
//===------------------------------------------------------------------===//

llvm::SmallVector<std::string, 16> Substitutions;
unsigned SubstSeqNo = 0;

bool shouldAddSubstitution(llvm::StringRef Component) const {
  return Component.size() > 1;
}

unsigned addSubstitution(llvm::StringRef Component) {
  unsigned Idx = SubstSeqNo++;
  Substitutions.push_back(Component.str());
  return Idx;
}

// 索引 0 → "S_", 索引 1 → "S0_", 索引 37 → "SA_"
std::string getSubstitutionEncoding(unsigned Idx) const {
  if (Idx == 0) return "S_";
  std::string Enc;
  unsigned V = Idx - 1;
  if (V < 36) {
    Enc += (V < 10) ? ('0' + V) : ('A' + V - 10);
  } else {
    llvm::SmallString<8> Buf;
    while (V > 0) {
      unsigned Rem = V % 36;
      Buf.push_back(Rem < 10 ? ('0' + Rem) : ('A' + Rem - 10));
      V /= 36;
    }
    Enc.append(Buf.rbegin(), Buf.rend());
  }
  Enc += '_';
  return Enc;
}

llvm::Optional<std::string> trySubstitution(llvm::StringRef Component) const {
  for (unsigned I = 0; I < Substitutions.size(); ++I) {
    if (Substitutions[I] == Component)
      return getSubstitutionEncoding(I);
  }
  return llvm::None;
}

void resetSubstitutions() {
  Substitutions.clear();
  SubstSeqNo = 0;
}
```

**步骤 2：集成 substitution 到 mangleNestedName / mangleRecordType（2h）**

修改 `src/CodeGen/Mangler.cpp`：

```cpp
void Mangler::mangleNestedName(const CXXRecordDecl *RD, std::string &Out) {
  llvm::SmallVector<llvm::StringRef, 4> NsChain;
  // ... 现有收集逻辑保持不变 ...

  for (auto It = NsChain.rbegin(); It != NsChain.rend(); ++It) {
    // ★ std 命名空间使用 St 缩写
    if (*It == "std") { Out += "St"; continue; }
    // ★ 编码前先查找 substitution
    if (auto Subst = trySubstitution(*It)) {
      Out += *Subst;
    } else {
      mangleSourceName(*It, Out);
      if (shouldAddSubstitution(*It)) addSubstitution(*It);
    }
  }

  llvm::StringRef ClassName = RD->getName();
  if (auto Subst = trySubstitution(ClassName)) {
    Out += *Subst;
  } else {
    mangleSourceName(ClassName, Out);
    if (shouldAddSubstitution(ClassName)) addSubstitution(ClassName);
  }
}
```

同样修改 `mangleRecordType`：

```cpp
void Mangler::mangleRecordType(const RecordType *T, std::string &Out) {
  llvm::StringRef Name = T->getDecl()->getName();
  if (auto Subst = trySubstitution(Name)) { Out += *Subst; return; }
  mangleSourceName(Name, Out);
  if (shouldAddSubstitution(Name)) addSubstitution(Name);
}
```

**步骤 3：在顶层入口重置 substitution（0.5h）**

```cpp
std::string Mangler::getMangledName(const FunctionDecl *FD) {
  if (!FD) return "";
  resetSubstitutions();  // ★ 每个顶层 mangle 调用时重置
  // ... 现有逻辑不变 ...
}
```

**步骤 4：改进模板表达式编码（2h）**

```cpp
void Mangler::mangleTemplateSpecializationType(
    const TemplateSpecializationType *T, std::string &Out) {
  // ★ 改进：使用标准 I...E 编码替代 vendor extension u5tparam
  Out += 'I';
  for (unsigned I = 0; I < T->getNumArgs(); ++I) {
    mangleQualType(T->getArg(I), Out);
  }
  Out += 'E';
}
```

**步骤 5：编写测试（1.5h）**

- `std::foo` → `_ZSt3foo`（St 缩写）而非 `_ZN3std3fooE`
- `ns::Foo::bar` 重复引用 → substitution 回指（`S_`/`S0_`）
- `std::vector<int>` → 模板 `I...E` 编码
- 回归测试：所有现有 Mangler 测试通过

#### 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|---------|
| substitution 时机错误（过早或过晚记录） | 高 | 用 `c++filt` 逐一验证；参照 Clang `ItaniumMangleContextImpl` |
| 标准实体编码冲突 | 低 | `St`/`Sa` 由 Itanium ABI 规范定义 |
| 模板参数编码遗漏 | 中 | 先支持类型参数，非类型参数后续迭代 |

#### 验收标准

```
[ ] Mangler substitution 压缩工作（c++filt 验证）
[ ] std::foo → _ZSt3foo（St 缩写）
[ ] 重复命名空间/类名使用 S_/S0_ 回指
[ ] 模板类型使用 I...E 标准编码
[ ] 所有现有 Mangler 测试仍通过
[ ] 新增至少 8 个 substitution 相关测试用例
```

---

### Task 8.2.1 — 内建函数（11h）

**目标：** 实现 GCC/Clang 兼容的常用内建函数

**当前差距：**
- 无 `Builtins.def`/`Builtins.h`/`Builtins.cpp`
- Sema 不识别 `__builtin_` 前缀
- CodeGen 无 `EmitBuiltinCall()` 方法

#### 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/blocktype/Basic/Builtins.def` | **新建** | 内建函数定义表 |
| `include/blocktype/Basic/Builtins.h` | **新建** | 内建函数查询接口 |
| `src/Basic/Builtins.cpp` | **新建** | 内建函数实现 |
| `src/Basic/CMakeLists.txt` | 修改 | 添加 `Builtins.cpp` |
| `src/Sema/SemaExpr.cpp` | 修改 | 识别 `__builtin_` 前缀 |
| `src/CodeGen/CodeGenExpr.cpp` | 修改 | 添加 `EmitBuiltinCall()` |
| `include/blocktype/CodeGen/CodeGenFunction.h` | 修改 | 声明 `EmitBuiltinCall()` |
| `include/blocktype/AST/Expr.h` | 修改 | `CallExpr` 添加 `IsBuiltinCall` 标志 |
| `include/blocktype/AST/ASTContext.h` | 修改 | 添加 `createImplicitBuiltinDecl()` |
| `tests/CodeGen/builtin_functions.cpp` | **新建** | 端到端测试 |

#### 详细开发步骤

**步骤 1：创建 Builtins.def 定义表（2h）**

```cpp
// include/blocktype/Basic/Builtins.def — 新建
// BUILTIN(Name, Type, Attributes)
//   Type: i=int, l=long, x=long long, v=void, b=bool, z=size_t
//         U=unsigned, P=pointer, K=const
//   Attrs: n=nothrow, c=const, r=noreturn

// 通用
BUILTIN(__builtin_abs,         "i.i",    "nc")
BUILTIN(__builtin_labs,        "l.l",    "nc")
BUILTIN(__builtin_llabs,       "x.x",    "nc")

// 位操作
BUILTIN(__builtin_ctz,         "iUi",    "nc")
BUILTIN(__builtin_ctzl,        "iUl",    "nc")
BUILTIN(__builtin_ctzll,       "iUx",    "nc")
BUILTIN(__builtin_clz,         "iUi",    "nc")
BUILTIN(__builtin_clzl,        "iUl",    "nc")
BUILTIN(__builtin_clzll,       "iUx",    "nc")
BUILTIN(__builtin_popcount,    "iUi",    "nc")
BUILTIN(__builtin_popcountl,   "iUl",    "nc")
BUILTIN(__builtin_popcountll,  "iUx",    "nc")

// 字节交换
BUILTIN(__builtin_bswap16,     "UsUs",   "nc")
BUILTIN(__builtin_bswap32,     "UiUi",   "nc")
BUILTIN(__builtin_bswap64,     "UxUx",   "nc")

// 控制流
BUILTIN(__builtin_expect,      "x.xb",   "nc")
BUILTIN(__builtin_unreachable, "v",      "nr")
BUILTIN(__builtin_trap,        "v",      "nr")

// 字符串/内存
BUILTIN(__builtin_strlen,      "zPKc",   "nc")
BUILTIN(__builtin_memcpy,      "v*v*Pvz", "n")
BUILTIN(__builtin_memset,      "v*Pviz",  "n")
BUILTIN(__builtin_memmove,     "v*v*Pvz", "n")

// 类型特征
BUILTIN(__builtin_is_constant_evaluated, "bv", "nc")

#undef BUILTIN
```

**步骤 2：创建 Builtins.h 查询接口（1.5h）**

```cpp
// include/blocktype/Basic/Builtins.h — 新建
#pragma once
#include "llvm/ADT/StringRef.h"

namespace blocktype {

enum class BuiltinID : unsigned {
  NotBuiltin = 0,
#define BUILTIN(ID, TYPE, ATTRS) ID,
#include "blocktype/Basic/Builtins.def"
  FirstTSBuiltin
};

struct BuiltinInfo {
  const char *Name;
  const char *Type;
  const char *Attrs;
};

class Builtins {
public:
  static bool isBuiltin(llvm::StringRef Name);
  static BuiltinID lookup(llvm::StringRef Name);
  static const BuiltinInfo &getInfo(BuiltinID ID);
  static llvm::StringRef getLLVMIntrinsicName(BuiltinID ID);
  static bool isNoThrow(BuiltinID ID);
  static bool isConst(BuiltinID ID);
  static bool isNoReturn(BuiltinID ID);

private:
  static const BuiltinInfo BuiltinTable[];
  static constexpr unsigned NumBuiltins =
      static_cast<unsigned>(BuiltinID::FirstTSBuiltin);
};

} // namespace blocktype
```

**步骤 3：创建 Builtins.cpp 实现（1.5h）**

```cpp
// src/Basic/Builtins.cpp — 新建
#include "blocktype/Basic/Builtins.h"
#include "llvm/ADT/StringMap.h"

namespace blocktype {

const BuiltinInfo Builtins::BuiltinTable[] = {
#define BUILTIN(ID, TYPE, ATTRS) {#ID, TYPE, ATTRS},
#include "blocktype/Basic/Builtins.def"
};

static llvm::StringMap<BuiltinID> &getBuiltinMap() {
  static llvm::StringMap<BuiltinID> Map;
  if (Map.empty()) {
#define BUILTIN(ID, TYPE, ATTRS) Map[#ID] = BuiltinID::ID;
#include "blocktype/Basic/Builtins.def"
  }
  return Map;
}

bool Builtins::isBuiltin(llvm::StringRef Name) {
  return Name.startswith("__builtin_") && lookup(Name) != BuiltinID::NotBuiltin;
}

BuiltinID Builtins::lookup(llvm::StringRef Name) {
  auto It = getBuiltinMap().find(Name);
  return (It != getBuiltinMap().end()) ? It->second : BuiltinID::NotBuiltin;
}

const BuiltinInfo &Builtins::getInfo(BuiltinID ID) {
  return BuiltinTable[static_cast<unsigned>(ID) - 1];
}

bool Builtins::isNoThrow(BuiltinID ID) {
  auto &Info = getInfo(ID);
  return Info.Attrs && std::strchr(Info.Attrs, 'n');
}

bool Builtins::isConst(BuiltinID ID) {
  auto &Info = getInfo(ID);
  return Info.Attrs && std::strchr(Info.Attrs, 'c');
}

bool Builtins::isNoReturn(BuiltinID ID) {
  auto &Info = getInfo(ID);
  return Info.Attrs && std::strchr(Info.Attrs, 'r');
}

llvm::StringRef Builtins::getLLVMIntrinsicName(BuiltinID ID) {
  switch (ID) {
  case BuiltinID::__builtin_abs:         return "llvm.abs";
  case BuiltinID::__builtin_ctz:
  case BuiltinID::__builtin_ctzl:
  case BuiltinID::__builtin_ctzll:       return "llvm.cttz";
  case BuiltinID::__builtin_clz:
  case BuiltinID::__builtin_clzl:
  case BuiltinID::__builtin_clzll:       return "llvm.ctlz";
  case BuiltinID::__builtin_popcount:
  case BuiltinID::__builtin_popcountl:
  case BuiltinID::__builtin_popcountll:  return "llvm.ctpop";
  case BuiltinID::__builtin_bswap16:
  case BuiltinID::__builtin_bswap32:
  case BuiltinID::__builtin_bswap64:     return "llvm.bswap";
  case BuiltinID::__builtin_trap:        return "llvm.trap";
  case BuiltinID::__builtin_memcpy:      return "llvm.memcpy";
  case BuiltinID::__builtin_memset:      return "llvm.memset";
  case BuiltinID::__builtin_memmove:     return "llvm.memmove";
  default:                               return "";
  }
}

} // namespace blocktype
```

**步骤 4：修改 src/Basic/CMakeLists.txt（0.5h）**

```cmake
add_library(blocktype-basic
  Builtins.cpp          # ★ 新增
  Diagnostics.cpp
  FileManager.cpp
  Language.cpp
  SourceManager.cpp
  Translation.cpp
  Unicode.cpp
  UTF8Validator.cpp
)
```

**步骤 5：Sema 识别内建函数（2h）**

修改 `src/Sema/SemaExpr.cpp`：

```cpp
#include "blocktype/Basic/Builtins.h"

// 在 ActOnDeclRefExpr 中添加：
ExprResult Sema::ActOnDeclRefExpr(SourceLocation Loc, llvm::StringRef Name) {
  // ★ 新增：__builtin_ 前缀 → 隐式 FunctionDecl
  if (Name.startswith("__builtin_") && Builtins::isBuiltin(Name)) {
    BuiltinID ID = Builtins::lookup(Name);
    auto *FD = Context.createImplicitBuiltinDecl(ID, Name);
    auto *DRE = Context.create<DeclRefExpr>(Loc, FD, FD->getType());
    return ExprResult(DRE);
  }
  // ... 现有逻辑 ...
}
```

需要在 `ASTContext.h` 中添加：

```cpp
// 创建内建函数的隐式 FunctionDecl
FunctionDecl *createImplicitBuiltinDecl(BuiltinID ID, llvm::StringRef Name);
```

在 `CallExpr` 中添加标志：

```cpp
// include/blocktype/AST/Expr.h — CallExpr 添加
bool IsBuiltinCall = false;
bool isBuiltinCall() const { return IsBuiltinCall; }
void setIsBuiltinCall(bool V = true) { IsBuiltinCall = V; }
```

**步骤 6：CodeGen 生成内建调用（3h）**

在 `CodeGenFunction.h` 中声明：

```cpp
llvm::Value *EmitBuiltinCall(const CallExpr *CE, llvm::StringRef BuiltinName);
```

在 `CodeGenExpr.cpp` 中实现：

```cpp
#include "blocktype/Basic/Builtins.h"
#include "llvm/IR/Intrinsics.h"

llvm::Value *CodeGenFunction::EmitBuiltinCall(const CallExpr *CE,
                                               llvm::StringRef BuiltinName) {
  BuiltinID ID = Builtins::lookup(BuiltinName);
  if (ID == BuiltinID::NotBuiltin) return nullptr;

  llvm::SmallVector<llvm::Value *, 8> Args;
  for (unsigned I = 0; I < CE->getNumArgs(); ++I)
    Args.push_back(EmitExpr(CE->getArg(I)));

  switch (ID) {
  case BuiltinID::__builtin_trap:
    Builder.CreateCall(llvm::Intrinsic::getDeclaration(
        &CGM.getModule(), llvm::Intrinsic::trap));
    Builder.CreateUnreachable();
    return nullptr;

  case BuiltinID::__builtin_unreachable:
    Builder.CreateUnreachable();
    return nullptr;

  case BuiltinID::__builtin_abs: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
                                               llvm::Intrinsic::abs);
    auto *IsPoison = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(getLLVMContext()), 0);
    return Builder.CreateCall(F, {Args[0], IsPoison});
  }

  case BuiltinID::__builtin_ctz:
  case BuiltinID::__builtin_ctzl:
  case BuiltinID::__builtin_ctzll: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::cttz, {Args[0]->getType()});
    auto *IsPoison = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(getLLVMContext()), 0);
    return Builder.CreateCall(F, {Args[0], IsPoison});
  }

  case BuiltinID::__builtin_clz:
  case BuiltinID::__builtin_clzl:
  case BuiltinID::__builtin_clzll: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::ctlz, {Args[0]->getType()});
    auto *IsPoison = llvm::ConstantInt::get(
        llvm::Type::getInt1Ty(getLLVMContext()), 0);
    return Builder.CreateCall(F, {Args[0], IsPoison});
  }

  case BuiltinID::__builtin_popcount:
  case BuiltinID::__builtin_popcountl:
  case BuiltinID::__builtin_popcountll: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::ctpop, {Args[0]->getType()});
    return Builder.CreateCall(F, {Args[0]});
  }

  case BuiltinID::__builtin_bswap16:
  case BuiltinID::__builtin_bswap32:
  case BuiltinID::__builtin_bswap64: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::bswap, {Args[0]->getType()});
    return Builder.CreateCall(F, {Args[0]});
  }

  case BuiltinID::__builtin_expect:
    return Args[0];  // 优化提示，直接返回

  case BuiltinID::__builtin_is_constant_evaluated:
    return llvm::ConstantInt::getFalse(
        llvm::Type::getInt1Ty(getLLVMContext()));

  case BuiltinID::__builtin_memcpy: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::memcpy,
        {Args[0]->getType(), Args[1]->getType(), Args[2]->getType()});
    return Builder.CreateCall(F, Args);
  }

  case BuiltinID::__builtin_memset: {
    auto *F = llvm::Intrinsic::getDeclaration(&CGM.getModule(),
        llvm::Intrinsic::memset,
        {Args[0]->getType(), Args[2]->getType()});
    return Builder.CreateCall(F, {Args[0], Args[1], Args[2]});
  }

  default:
    return nullptr;  // 未实现 → 回退到普通函数调用
  }
}
```

在 `EmitCallExpr` 中添加内建检测：

```cpp
llvm::Value *CodeGenFunction::EmitCallExpr(const CallExpr *CE) {
  // ★ 新增：检测内建函数
  if (CE->isBuiltinCall()) {
    if (auto *DRE = llvm::dyn_cast<DeclRefExpr>(CE->getCallee())) {
      if (auto *V = EmitBuiltinCall(CE, DRE->getDecl()->getName()))
        return V;
    }
  }
  // ... 现有逻辑 ...
}
```

**步骤 7：编写测试（1h）**

```cpp
// tests/CodeGen/builtin_functions.cpp
int test_abs() { return __builtin_abs(-42); }
int test_ctz() { return __builtin_ctz(8); }
int test_clz() { return __builtin_clz(1); }
int test_popcount() { return __builtin_popcount(0xFF); }
unsigned test_bswap() { return __builtin_bswap32(0x12345678); }
long test_expect(long x) { return __builtin_expect(x, 0); }
void test_trap() { __builtin_trap(); }
bool test_const_eval() { return __builtin_is_constant_evaluated(); }
unsigned long test_strlen(const char *s) { return __builtin_strlen(s); }
```

#### 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|---------|
| 类型签名平台差异（`size_t` 32/64 位） | 中 | 使用 `TargetInfo` 查询类型大小 |
| `CallExpr::isBuiltinCall` 需修改 AST | 中 | 添加 `bool IsBuiltinCall` 标志位 |
| LLVM Intrinsic API 版本差异 | 低 | `llvm::Intrinsic::getDeclaration` 是稳定 API |
| `ASTContext::createImplicitBuiltinDecl` 需新增 | 中 | 参照现有 `createImplicit*` 方法模式 |

#### 验收标准

```
[ ] Builtins.def 定义表创建，包含 22+ 个内建函数
[ ] Builtins::isBuiltin/lookup/getInfo 工作正确
[ ] Sema 识别 __builtin_ 前缀并创建隐式 FunctionDecl
[ ] CodeGen EmitBuiltinCall 正确映射到 LLVM Intrinsic
[ ] __builtin_abs/ctz/clz/popcount/bswap 工作
[ ] __builtin_trap/unreachable/is_constant_evaluated 工作
[ ] __builtin_memcpy/memset/memmove 工作
[ ] 9 个端到端测试用例通过
[ ] 所有现有测试通过
```

---

### Task 8.2.2 — 调用约定（8h）

**目标：** 在 LLVM IR 层面正确设置调用约定属性

**当前差距：**
- ✅ `sret`/`inreg` 已在 `GetOrCreateFunctionDecl()` 中设置
- ❌ `calling convention` 未显式设置
- ❌ `signext`/`zeroext` 未设置
- ❌ `byval` 未设置
- ❌ 间接调用 calling convention 缺失

#### 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/CodeGen/CodeGenModule.cpp` | 修改 | 设置 calling convention + signext/zeroext/byval |
| `src/CodeGen/CodeGenFunction.cpp` | 修改 | 返回值属性 + 间接调用 calling convention |
| `include/blocktype/CodeGen/CodeGenTypes.h` | 修改 | 添加 `shouldSignExtend`/`shouldZeroExtend` |
| `src/CodeGen/CodeGenTypes.cpp` | 修改 | 实现 sign/zero extend 判断 |
| `tests/CodeGen/calling_convention.cpp` | **新建** | 测试 |

#### 详细开发步骤

**步骤 1：函数 calling convention（1.5h）**

修改 `src/CodeGen/CodeGenModule.cpp` 的 `GetOrCreateFunctionDecl()`：

```cpp
// 在 Fn 创建后添加：
Fn->setCallingConv(llvm::CallingConv::C);
```

**步骤 2：添加 shouldSignExtend/shouldZeroExtend（1h）**

在 `CodeGenTypes.h` 中添加：

```cpp
/// 判断参数类型是否需要 signext（有符号小整数 i1/i8/i16）
bool shouldSignExtend(QualType Ty) const;

/// 判断参数类型是否需要 zeroext（无符号小整数 i1/i8/i16）
bool shouldZeroExtend(QualType Ty) const;
```

在 `CodeGenTypes.cpp` 中实现：

```cpp
bool CodeGenTypes::shouldSignExtend(QualType Ty) const {
  if (Ty.isNull()) return false;
  auto *BT = llvm::dyn_cast<BuiltinType>(Ty.getTypePtr());
  if (!BT) return false;
  // x86_64: 有符号 i1/i8/i16 需要 signext
  // AArch64: 不需要（AAPCS64 自动扩展）
  if (!CGM.getTargetInfo().isX86_64()) return false;
  if (!BT->isSignedInteger()) return false;
  unsigned Width = CGM.getTargetInfo().getBuiltinSize(BT->getKind()) * 8;
  return Width < 32;  // i1/i8/i16
}

bool CodeGenTypes::shouldZeroExtend(QualType Ty) const {
  if (Ty.isNull()) return false;
  auto *BT = llvm::dyn_cast<BuiltinType>(Ty.getTypePtr());
  if (!BT) return false;
  if (!CGM.getTargetInfo().isX86_64()) return false;
  if (BT->isSignedInteger()) return false;
  if (!BT->isUnsignedInteger()) return false;
  unsigned Width = CGM.getTargetInfo().getBuiltinSize(BT->getKind()) * 8;
  return Width < 32;
}
```

**步骤 3：参数属性增强（2h）**

修改 `GetOrCreateFunctionDecl()` 中的参数属性设置：

```cpp
// 在现有 sret/inreg 处理后添加：

// ★ signext/zeroext
if (Info.isDirect()) {
  llvm::Type *ArgTy = Arg.getType();
  if (auto *IntTy = llvm::dyn_cast<llvm::IntegerType>(ArgTy)) {
    unsigned BitWidth = IntTy->getBitWidth();
    if (BitWidth < 32) {
      // 获取原始参数类型
      unsigned RealParamIdx = ArgIdx - ImplicitArgs;
      if (RealParamIdx < FD->getNumParams()) {
        QualType ParamTy = FD->getParamDecl(RealParamIdx)->getType();
        if (getTypes().shouldSignExtend(ParamTy))
          Arg.addAttr(llvm::Attribute::SExt);
        else if (getTypes().shouldZeroExtend(ParamTy))
          Arg.addAttr(llvm::Attribute::ZExt);
      }
    }
  }
}
```

**步骤 4：返回值属性（1.5h）**

```cpp
// 在 GetOrCreateFunctionDecl() 中，Fn 属性设置区域添加：

// ★ 返回值 signext/zeroext（属性索引 0）
if (ABI && ABI->RetInfo.isDirect()) {
  QualType RetTy = FD->getReturnType();
  if (getTypes().shouldSignExtend(RetTy))
    Fn->addRetAttr(llvm::Attribute::SExt);
  else if (getTypes().shouldZeroExtend(RetTy))
    Fn->addRetAttr(llvm::Attribute::ZExt);
}
```

**步骤 5：间接调用处理（1.5h）**

修改 `src/CodeGen/CodeGenFunction.cpp` 中的间接调用生成：

```cpp
// 间接调用（函数指针/虚函数）时设置 calling convention
llvm::CallInst *Call = Builder.CreateCall(Callee, Args);
Call->setCallingConv(llvm::CallingConv::C);  // ★ 新增

// 传播参数属性（从函数类型推断）
if (auto *FnTy = llvm::dyn_cast<llvm::FunctionType>(
    Callee->getType()->getPointerElementType())) {
  for (unsigned I = 0; I < Args.size(); ++I) {
    // 传播 sret/inreg/signext/zeroext 属性
  }
}
```

**步骤 6：编写测试（0.5h）**

```cpp
// tests/CodeGen/calling_convention.cpp

// 测试 1: signext 参数
int test_signext(signed char x) { return x; }
// 预期 x86_64: define signext i32 @test_signext(i8 signext %x)

// 测试 2: zeroext 参数
int test_zeroext(unsigned char x) { return x; }
// 预期 x86_64: define i32 @test_zeroext(i8 zeroext %x)

// 测试 3: signext 返回值
signed char test_ret_signext() { return -1; }
// 预期 x86_64: define signext i8 @test_ret_signext()

// 测试 4: calling convention
void test_cc() {}
// 预期: define void @test_cc() #0  (callingconv C)

// 测试 5: 间接调用
void test_indirect(void (*fp)()) { fp(); }
// 预期: call void %fp()  (callingconv C)

// 测试 6: AArch64 不需要 signext
// （交叉编译测试）
```

#### 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|---------|
| signext/zeroext 平台差异 | 中 | x86_64 需要，AArch64 不需要；用 `TargetInfo::isX86_64()` 判断 |
| byval 属性与现有 ABI 分类冲突 | 低 | 当前 `ABIArgInfo` 无 `ByVal` kind，后续迭代添加 |
| 间接调用属性传播不完整 | 中 | 先设置 calling convention，属性传播后续完善 |

#### 验收标准

```
[ ] 函数 calling convention 正确设置（C conv）
[ ] signext 属性正确（x86_64 有符号 i1/i8/i16）
[ ] zeroext 属性正确（x86_64 无符号 i1/i8/i16）
[ ] 返回值 signext/zeroext 正确
[ ] 间接调用 calling convention 正确
[ ] AArch64 不添加 signext/zeroext
[ ] 所有现有测试通过
```

---

### Wave 2 执行顺序

**建议：8.1.3 → 8.2.1 → 8.2.2**

| Task | 时间 | 依赖 |
|------|------|------|
| 8.1.3 | 9h | 8.1.1 ✅ |
| 8.2.1 | 11h | 8.1.1 ✅ |
| 8.2.2 | 8h | 8.1.2 ✅ |

**Wave 2 总计：28h（约 3.5 个工作日）**

---

## 三、Wave 3 详细计划

### Task 8.3.1 — 基本优化（8h）

**目标：** 集成 LLVM PassBuilder，实现 -O0/-O1/-O2/-O3

**当前差距：**
- `runOptimizationPasses()` 空壳（仅 warning）
- `generateObjectFile()` 空壳（仅输出 LLVM IR）

#### 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/Frontend/CompilerInstance.cpp` | 修改 | 实现两个空壳方法 |
| `tools/driver.cpp` | 修改 | LLVM Target 初始化 |
| `tests/CodeGen/optimization.cpp` | **新建** | 测试 |

#### 详细开发步骤

**步骤 1：实现 runOptimizationPasses()（3h）**

替换 `src/Frontend/CompilerInstance.cpp` 中的空壳实现：

```cpp
```cpp
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

bool CompilerInstance::runOptimizationPasses(llvm::Module &Module) {
  unsigned OptLevel = Invocation->CodeGenOpts.OptimizationLevel;
  if (OptLevel == 0) return true;  // O0: 无优化

  // 创建 PassBuilder
  llvm::PassBuilder PB;

  // 注册 Analysis Manager
  llvm::LoopAnalysisManager LAM;
  llvm::FunctionAnalysisManager FAM;
  llvm::CGSCCAnalysisManager CGAM;
  llvm::ModuleAnalysisManager MAM;

  // 注册标准 analyses
  PB.registerModuleAnalyses(MAM);
  PB.registerCGSCCAnalyses(CGAM);
  PB.registerFunctionAnalyses(FAM);
  PB.registerLoopAnalyses(LAM);
  PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

  // 根据优化级别构建 pipeline
  llvm::OptimizationLevel Level;
  switch (OptLevel) {
  case 1:  Level = llvm::OptimizationLevel::O1; break;
  case 2:  Level = llvm::OptimizationLevel::O2; break;
  case 3:  Level = llvm::OptimizationLevel::O3; break;
  default: Level = llvm::OptimizationLevel::O2; break;
  }

  auto MPM = PB.buildPerModuleDefaultPipeline(Level);
  MPM.run(Module, MAM);

  return true;
}
```

**步骤 2：实现 generateObjectFile()（3h）**

```cpp
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"

bool CompilerInstance::generateObjectFile(llvm::Module &Module,
                                          StringRef OutputPath) {
  // 获取目标三元组
  auto TargetTriple = Module.getTargetTriple();
  if (TargetTriple.empty())
    TargetTriple = llvm::sys::getDefaultTargetTriple();
  Module.setTargetTriple(TargetTriple);

  // 查找目标
  std::string Error;
  auto *Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
    errs() << "Error: " << Error << "\n";
    return false;
  }

  // 创建 TargetMachine
  llvm::TargetOptions Opt;
  auto RM = llvm::Reloc::PIC_;  // 默认 PIC
  auto CM = Invocation->CodeGenOpts.OptimizationLevel >= 2
                ? llvm::CodeGenOptLevel::Default
                : llvm::CodeGenOptLevel::None;

  std::string CPU = Invocation->TargetOpts.CPU;
  std::string Features = Invocation->TargetOpts.Features;

  auto TM = Target->createTargetMachine(TargetTriple, CPU, Features, Opt,
                                         RM, llvm::CodeModel::Default, CM);
  if (!TM) {
    errs() << "Error: Could not create TargetMachine\n";
    return false;
  }

  Module.setDataLayout(TM->createDataLayout());

  // 输出到文件
  std::error_code EC;
  llvm::raw_fd_ostream Out(OutputPath, EC, llvm::sys::fs::OF_None);
  if (EC) {
    errs() << "Error: Cannot open output file '" << OutputPath
           << "': " << EC.message() << "\n";
    return false;
  }

  // 生成目标代码
  llvm::legacy::PassManager PM;
  if (TM->addPassesToEmitFile(PM, Out, nullptr,
                               llvm::CodeGenFileType::ObjectFile)) {
    errs() << "Error: TargetMachine can't emit object file\n";
    return false;
  }

  PM.run(Module);
  Out.flush();
  return true;
}
```

**步骤 3：driver.cpp 初始化（1h）**

```cpp
// tools/driver.cpp — 在 main() 开头添加：

// ★ 初始化 LLVM Target 组件（必须在任何 Target 操作之前调用）
llvm::InitializeAllTargets();
llvm::InitializeAllTargetMCs();
llvm::InitializeAllAsmPrinters();
llvm::InitializeAllAsmParsers();
```

**步骤 4：编写测试（1h）**

```cpp
// tests/CodeGen/optimization.cpp

// 测试 1: O0 无优化
int test_o0(int x) { return x + 1; }
// 预期: 无优化变换

// 测试 2: O1 基本优化
int test_o1(int x) {
  int a = 1;
  int b = 2;
  return x + a + b;  // 常量折叠 → x + 3
}

// 测试 3: O2 标准优化
int test_o2(int x) { return x * 2 + x * 3; }  // → x * 5

// 测试 4: O3 激进优化
void test_o3(int *p, int n) {
  for (int i = 0; i < n; ++i) p[i] = 0;  // 可能向量化
}

// 测试 5: 目标代码生成
int test_obj() { return 42; }
// 预期: 生成有效 .o 文件（ELF/Mach-O）
```

#### 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|---------|
| LLVM PassBuilder API 版本差异 | 中 | 检查 `LLVM_VERSION_MAJOR` 宏 |
| legacy vs 新 PassManager | 中 | `addPassesToEmitFile` 可能仍需 legacy PassManager |
| TargetMachine 初始化遗漏 | 中 | 在 `driver.cpp` 的 `main()` 中尽早初始化 |
| 交叉编译目标未安装 | 低 | 检查 `lookupTarget` 返回值 |

#### 验收标准

```
[ ] -O0 无优化（直接返回）
[ ] -O1/-O2/-O3 使用 PassBuilder 正确优化
[ ] generateObjectFile() 生成有效 .o 文件
[ ] macOS ARM64 目标代码正确
[ ] Linux x86_64 目标代码正确（交叉编译，如果目标已安装）
[ ] 所有现有测试通过
```

---

### Task 8.3.2 — 目标特定优化（7.5h）

**目标：** 实现目标平台特定的优化配置

#### 文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/Frontend/CompilerInstance.cpp` | 修改 | 目标特定优化配置 |
| `include/blocktype/Frontend/CompilerInvocation.h` | 修改 | 扩展 TargetOptions |
| `tests/CodeGen/target_optimization.cpp` | **新建** | 测试 |

#### 详细开发步骤

**步骤 1：扩展 TargetOptions（1h）**

修改 `include/blocktype/Frontend/CompilerInvocation.h`：

```cpp
struct TargetOptions {
  std::string Triple;
  std::string CPU;
  std::string Features;
  std::string ABI;

  // ★ 新增
  std::string FloatABI = "hard";    // 浮点 ABI: "hard"/"soft"/"softfp"
  bool PIE = true;                   // 默认 PIE
  std::string CodeModel = "default"; // 代码模型
};
```

**步骤 2：x86_64 优化配置（2h）**

修改 `CompilerInstance::generateObjectFile()` 中的 TargetMachine 创建：

```cpp
// x86_64 默认配置
if (TargetTriple.startswith("x86_64")) {
  if (CPU.empty()) CPU = "x86-64-v2";  // SSE4.2
  if (Features.empty()) Features = "+sse4.2,+cx16,+popcnt";
  // FloatABI: x86_64 始终 hard
}
```

**步骤 3：AArch64 优化配置（2h）**

```cpp
// AArch64 默认配置
if (TargetTriple.startswith("aarch64") || TargetTriple.startswith("arm64")) {
  if (CPU.empty()) {
    CPU = TargetTriple.contains("apple") ? "apple-m1" : "generic";
  }
  if (Features.empty()) Features = "+neon,+fp-armv8";
  // AAPCS64 硬浮点
}
```

**步骤 4：TargetMachine 优化配置（1.5h）**

```cpp
// PIC/PIE 模式
auto RM = Invocation->TargetOpts.PIE
              ? llvm::Reloc::PIC_
              : llvm::Reloc::Static;

// CodeGenOptLevel
auto CM = [&]() -> llvm::CodeGenOptLevel {
  switch (Invocation->CodeGenOpts.OptimizationLevel) {
  case 0:  return llvm::CodeGenOptLevel::None;
  case 1:  return llvm::CodeGenOptLevel::Less;
  case 2:  return llvm::CodeGenOptLevel::Default;
  case 3:  return llvm::CodeGenOptLevel::Aggressive;
  default: return llvm::CodeGenOptLevel::Default;
  }
}();

// 代码模型
auto CMModel = llvm::CodeModel::Default;
if (Invocation->TargetOpts.CodeModel == "small")
  CMModel = llvm::CodeModel::Small;
else if (Invocation->TargetOpts.CodeModel == "large")
  CMModel = llvm::CodeModel::Large;
```

**步骤 5：编写测试（1h）**

```cpp
// tests/CodeGen/target_optimization.cpp

// 测试 1: x86_64 默认 CPU
// 预期: -mcpu=x86-64-v2

// 测试 2: AArch64 macOS 默认 CPU
// 预期: -mcpu=apple-m1

// 测试 3: AArch64 Linux 默认 CPU
// 预期: -mcpu=generic

// 测试 4: PIE 模式
// 预期: Reloc::PIC_

// 测试 5: 硬浮点 ABI
// 预期: FloatABI = hard
```

#### 风险评估

| 风险 | 级别 | 缓解措施 |
|------|------|---------|
| CPU 特性字符串格式 | 低 | 参照 LLVM `TargetParser` |
| apple-m1 在旧 LLVM 版本不可用 | 中 | 回退到 "generic" |
| PIE 与静态链接冲突 | 低 | 仅在动态链接时启用 PIE |

#### 验收标准

```
[ ] x86_64 默认 CPU 为 x86-64-v2
[ ] AArch64 macOS 默认 CPU 为 apple-m1
[ ] AArch64 Linux 默认 CPU 为 generic
[ ] PIE 模式正确设置
[ ] 浮点 ABI 为硬浮点
[ ] CodeGenOptLevel 与 -O 级别对应
[ ] 所有现有测试通过
```

---

### Wave 3 执行顺序

**建议：8.3.1 → 8.3.2**（8.3.2 依赖 8.3.1 的 TargetMachine 基础）

| Task | 时间 | 依赖 |
|------|------|------|
| 8.3.1 | 8h | 8.1.2 ✅, 8.2.2 |
| 8.3.2 | 7.5h | 8.3.1 |

**Wave 3 总计：15.5h（约 2 个工作日）**

---

## 四、Task 7.3.2 Contract 语义状态评估

**当前状态：**
- ✅ 7.3.1 Contract 属性已完成（pre/post/assert 解析 + CodeGen 检查代码）
- ⚠️ 7.3.2 Contract 语义正在开发中

**7.3.2 需要实现：**
1. Contract 继承（派生类继承基类 Contract）
2. Contract 与虚函数交互
3. Contract 检查模式切换

**接口已预置：** `SemaCXX.h` 中已声明 `CheckContractInheritance()`、`InheritBaseContracts()`、`collectContracts()`

**对 Phase 8 的影响：** 无直接依赖。Contract 语义属于 Sema 层，Phase 8 属于 CodeGen/Target 层。两者可并行开发。

**建议：** 7.3.2 由 dev-tester 在 Phase 8 Wave 2 期间并行处理，不影响 Phase 8 进度。

---

## 五、关键风险汇总

| 风险 | 级别 | 影响 Task | 缓解措施 |
|------|------|----------|---------|
| System V AMD64 substitution 时机错误 | 中 | 8.1.3 | `c++filt` 逐一验证 |
| 内建函数类型签名平台差异 | 中 | 8.2.1 | `TargetInfo` 查询，参照 Clang |
| signext/zeroext 平台差异 | 中 | 8.2.2 | x86_64 需要 AArch64 不需要 |
| LLVM PassBuilder API 版本差异 | 中 | 8.3.1 | 检查 `LLVM_VERSION_MAJOR` |
| legacy vs 新 PassManager | 中 | 8.3.1 | `addPassesToEmitFile` 可能仍需 legacy |
| TargetMachine 初始化遗漏 | 中 | 8.3.1 | `driver.cpp` 尽早初始化 |
| CPU 特性字符串格式 | 低 | 8.3.2 | 参照 LLVM `TargetParser` |

---

## 六、工作量汇总

| Wave | Tasks | 预估时间 | 日历时间 |
|------|-------|---------|---------|
| Wave 1 | 8.1.1 + 8.1.2 | — | ✅ 已完成 |
| Wave 2 | 8.1.3 + 8.2.1 + 8.2.2 | 28h | 3.5 天 |
| Wave 3 | 8.3.1 + 8.3.2 | 15.5h | 2 天 |
| **总计** | — | **43.5h** | **5.5 天** |

---

## 七、验收检查清单

### Wave 2
```
[ ] Mangler substitution 压缩工作（c++filt 验证）
[ ] 标准实体预定义（St=std::）正确
[ ] Builtins.def 定义表创建
[ ] __builtin_abs/strlen/expect/ctz/clz/popcount/bswap 工作
[ ] __builtin_trap/unreachable/is_constant_evaluated 工作
[ ] 函数 calling convention 正确设置
[ ] signext/zeroext 属性正确
[ ] 间接调用 calling convention 正确
[ ] 所有现有测试通过（回归测试）
```

### Wave 3
```
[ ] -O0/-O1/-O2/-O3 优化级别工作
[ ] generateObjectFile() 生成有效 .o 文件
[ ] x86_64 默认 CPU 为 x86-64-v2
[ ] AArch64 macOS 默认 CPU 为 apple-m1
[ ] PIE 模式正确设置
[ ] 浮点 ABI 为硬浮点
[ ] macOS ARM64 目标代码正确
[ ] Linux x86_64 目标代码正确（交叉编译）
[ ] 所有现有测试通过（回归测试）
```

---

*Phase 8 完成标志：支持 Linux x86_64 和 macOS ARM64 目标平台；优化 Pass 正确工作；能生成可执行文件。*