# Task A.2-Fix：修复 IRInstruction 缺失的 ICmpPred/FCmpPred 存储

> 本文档是修复任务，解决 A.2 遗留的架构缺陷：`IRInstruction` 不存储比较谓词，导致序列化硬编码谓词、反序列化丢失谓词。

---

## 依赖

- A.2（IRValue、User、Use、Opcode 枚举、ICmpPred/FCmpPred 枚举）
- A.3（IRInstruction）
- A.6（VerifierPass，需更新验证逻辑）
- A.7（IRSerializer，需更新序列化/反序列化逻辑）

---

## 产出文件

| 操作 | 文件路径 | 说明 |
|------|----------|------|
| 修改 | `include/blocktype/IR/IRInstruction.h` | 添加 `Pred_` 字段 + getter/setter |
| 修改 | `src/IR/IRBuilder.cpp` | createICmp/createFCmp 中保存 Pred |
| 修改 | `src/IR/IRInstruction.cpp` | print() 中输出 predicate |
| 修改 | `src/IR/IRSerializer.cpp` | 移除硬编码，使用实际 predicate；解析器恢复 predicate |
| 修改 | `src/IR/IRVerifier.cpp` | 添加 predicate 合法性验证 |
| 修改 | `tests/unit/IR/IRBuilderTest.cpp` | 添加 predicate getter 测试 |
| 修改 | `tests/unit/IR/IRSerializerTest.cpp` | 添加 predicate round-trip 测试 |
| 修改 | `tests/unit/IR/IRVerifierTest.cpp` | 添加非法 predicate 验证测试 |

---

## 问题根因

### 现状

1. **IRBuilder.cpp** 第 121~133 行：`createICmp(ICmpPred Pred, ...)` 和 `createFCmp(FCmpPred Pred, ...)` 接收 Pred 参数但未保存到 IRInstruction 中（`Pred` 被丢弃）

2. **IRInstruction.h** 第 15~37 行：`IRInstruction` 类没有存储 predicate 的字段，只有 `Op`/`DialectID_`/`Parent` 三个字段

3. **IRSerializer.cpp** 第 300~311 行：writeText 中硬编码 `"eq"` 和 `"oeq"`
   ```cpp
   // TODO: IRInstruction does not store the predicate
   OS << "icmp eq " << ...   // 硬编码 eq
   OS << "fcmp oeq " << ...  // 硬编码 oeq
   ```

4. **IRSerializer.cpp** 第 1496~1512 行：parseText 中读取 PredStr 但丢弃
   ```cpp
   auto PredStr = readIdent(); // skip predicate
   (void)PredStr;
   ```

5. **IRInstruction.cpp** 第 50~91 行：print() 不输出 predicate

6. **IRVerifier.cpp** 第 528~553 行：verifyCmpOp() 不验证 predicate 合法性

### 影响

- 创建 `icmp sgt` 指令 → 序列化输出为 `icmp eq`（错误！）
- 反序列化 `icmp sgt` → 解析后无 predicate 信息
- 导致序列化 round-trip 不一致

---

## 修改规范（逐文件精确描述）

### 修改 1：IRInstruction.h — 添加 predicate 字段

**文件**：`include/blocktype/IR/IRInstruction.h`

**当前代码**（第 15~24 行）：
```cpp
class IRInstruction : public User {
  Opcode Op;
  dialect::DialectID DialectID_;
  IRBasicBlock* Parent;

public:
  IRInstruction(Opcode O, IRType* Ty, unsigned ID,
                dialect::DialectID D = dialect::DialectID::Core, StringRef N = "")
    : User(ValueKind::InstructionResult, Ty, ID, N),
      Op(O), DialectID_(D), Parent(nullptr) {}
```

**修改后**：
```cpp
class IRInstruction : public User {
  Opcode Op;
  dialect::DialectID DialectID_;
  uint8_t Pred_ = 0;    // ICmpPred 或 FCmpPred 的原始值；非比较指令为 0
  IRBasicBlock* Parent;

public:
  IRInstruction(Opcode O, IRType* Ty, unsigned ID,
                dialect::DialectID D = dialect::DialectID::Core, StringRef N = "")
    : User(ValueKind::InstructionResult, Ty, ID, N),
      Op(O), DialectID_(D), Pred_(0), Parent(nullptr) {}
```

**新增访问器**（在 `isComparison()` 之后添加）：
```cpp
  /// 获取 predicate 原始值（仅对 ICmp/FCmp 指令有意义）。
  uint8_t getPredicate() const { return Pred_; }
  /// 设置 predicate 原始值。
  void setPredicate(uint8_t P) { Pred_ = P; }
  /// 获取 ICmp predicate（仅对 Opcode::ICmp 有效）。
  ICmpPred getICmpPredicate() const { return static_cast<ICmpPred>(Pred_); }
  /// 获取 FCmp predicate（仅对 Opcode::FCmp 有效）。
  FCmpPred getFCmpPredicate() const { return static_cast<FCmpPred>(Pred_); }
```

**关键约束**：
- `Pred_` 类型为 `uint8_t`，足以容纳 `ICmpPred`（0~9）和 `FCmpPred`（0~15）
- 默认值 0 对应 `ICmpPred::EQ` 和 `FCmpPred::False`，向后兼容
- 构造函数签名不变，所有现有调用点无需修改
- `sizeof(IRInstruction)` 增加 1 字节（因对齐填充，实际可能不变）

---

### 修改 2：IRBuilder.cpp — 保存 predicate

**文件**：`src/IR/IRBuilder.cpp`

**createICmp 修改**（约第 121~126 行）：

当前：
```cpp
IRInstruction* IRBuilder::createICmp(ICmpPred Pred, IRValue* LHS, IRValue* RHS, StringRef Name) {
  auto I = std::make_unique<IRInstruction>(Opcode::ICmp, TypeCtx.getInt1Ty(), 0, dialect::DialectID::Core, Name);
  I->addOperand(LHS);
  I->addOperand(RHS);
  return insertHelper(std::move(I));
}
```

修改后：
```cpp
IRInstruction* IRBuilder::createICmp(ICmpPred Pred, IRValue* LHS, IRValue* RHS, StringRef Name) {
  auto I = std::make_unique<IRInstruction>(Opcode::ICmp, TypeCtx.getInt1Ty(), 0, dialect::DialectID::Core, Name);
  I->setPredicate(static_cast<uint8_t>(Pred));
  I->addOperand(LHS);
  I->addOperand(RHS);
  return insertHelper(std::move(I));
}
```

**createFCmp 修改**（约第 128~133 行）：

当前：
```cpp
IRInstruction* IRBuilder::createFCmp(FCmpPred Pred, IRValue* LHS, IRValue* RHS, StringRef Name) {
  auto I = std::make_unique<IRInstruction>(Opcode::FCmp, TypeCtx.getInt1Ty(), 0, dialect::DialectID::Core, Name);
  I->addOperand(LHS);
  I->addOperand(RHS);
  return insertHelper(std::move(I));
}
```

修改后：
```cpp
IRInstruction* IRBuilder::createFCmp(FCmpPred Pred, IRValue* LHS, IRValue* RHS, StringRef Name) {
  auto I = std::make_unique<IRInstruction>(Opcode::FCmp, TypeCtx.getInt1Ty(), 0, dialect::DialectID::Core, Name);
  I->setPredicate(static_cast<uint8_t>(Pred));
  I->addOperand(LHS);
  I->addOperand(RHS);
  return insertHelper(std::move(I));
}
```

---

### 修改 3：IRInstruction.cpp — print() 输出 predicate

**文件**：`src/IR/IRInstruction.cpp`

**当前代码**（第 50~91 行的 print 方法）中，在 `OS << "%" << getValueID() << " = " << Name;` 之后，添加 predicate 输出：

在操作数输出之前添加（约第 83 行之后）：
```cpp
  // 输出比较谓词
  if (Op == Opcode::ICmp) {
    OS << " " << icmpPredToText(getICmpPredicate());
  } else if (Op == Opcode::FCmp) {
    OS << " " << fcmpPredToText(getFCmpPredicate());
  }
```

需要在文件顶部添加 predicate 转字符串的辅助函数（可从 IRSerializer.cpp 复制，或提取为公共函数）：

```cpp
static const char* icmpPredToText(ICmpPred P) {
  switch (P) {
  case ICmpPred::EQ:  return "eq";
  case ICmpPred::NE:  return "ne";
  case ICmpPred::UGT: return "ugt";
  case ICmpPred::UGE: return "uge";
  case ICmpPred::ULT: return "ult";
  case ICmpPred::ULE: return "ule";
  case ICmpPred::SGT: return "sgt";
  case ICmpPred::SGE: return "sge";
  case ICmpPred::SLT: return "slt";
  case ICmpPred::SLE: return "sle";
  }
  return "unknown";
}

static const char* fcmpPredToText(FCmpPred P) {
  switch (P) {
  case FCmpPred::False: return "false";
  case FCmpPred::OEQ:   return "oeq";
  case FCmpPred::OGT:   return "ogt";
  case FCmpPred::OGE:   return "oge";
  case FCmpPred::OLT:   return "olt";
  case FCmpPred::OLE:   return "ole";
  case FCmpPred::ONE:   return "one";
  case FCmpPred::ORD:   return "ord";
  case FCmpPred::UNO:   return "uno";
  case FCmpPred::UEQ:   return "ueq";
  case FCmpPred::UGT:   return "ugt";
  case FCmpPred::UGE:   return "uge";
  case FCmpPred::ULT:   return "ult";
  case FCmpPred::ULE:   return "ule";
  case FCmpPred::UNE:   return "une";
  case FCmpPred::True:  return "true";
  }
  return "unknown";
}
```

---

### 修改 4：IRSerializer.cpp — 移除硬编码，使用实际 predicate

**文件**：`src/IR/IRSerializer.cpp`

#### 4a. writeInstructionText 中的 ICmp/FCmp case（约第 300~311 行）

当前：
```cpp
  case Opcode::ICmp:
    // TODO: IRInstruction does not store the predicate — createICmp/FCmp
    // accept a Pred parameter but discard it. Once IRInstruction gains
    // predicate support, use icmpPredToText() here.
    OS << "icmp eq " << typeToString(I.getOperand(0)->getType()) << " "
       << valueToString(I.getOperand(0)) << ", " << valueToString(I.getOperand(1));
    break;
  case Opcode::FCmp:
    // TODO: Same as ICmp — use fcmpPredToText() once predicates are stored.
    OS << "fcmp oeq " << typeToString(I.getOperand(0)->getType()) << " "
       << valueToString(I.getOperand(0)) << ", " << valueToString(I.getOperand(1));
    break;
```

修改后：
```cpp
  case Opcode::ICmp:
    OS << "icmp " << icmpPredToText(I.getICmpPredicate()) << " "
       << typeToString(I.getOperand(0)->getType()) << " "
       << valueToString(I.getOperand(0)) << ", " << valueToString(I.getOperand(1));
    break;
  case Opcode::FCmp:
    OS << "fcmp " << fcmpPredToText(I.getFCmpPredicate()) << " "
       << typeToString(I.getOperand(0)->getType()) << " "
       << valueToString(I.getOperand(0)) << ", " << valueToString(I.getOperand(1));
    break;
```

> 注：`icmpPredToText` 和 `fcmpPredToText` 已在 IRSerializer.cpp 第 139~175 行定义，无需重复。

#### 4b. parseText 中的 ICmp case（约第 1496~1512 行）

当前：
```cpp
            case Opcode::ICmp: {
              // icmp <pred> <type> <lhs>, <rhs>
              auto PredStr = readIdent(); // skip predicate
              (void)PredStr;
              skipWS();
              auto* Ty = parseType();
              skipWS();
              auto [L, _] = parseValueRef(ValMap, BBMap, F);
              skipWS();
              if (peek() == ',') advance();
              skipWS();
              auto [R, __] = parseValueRef(ValMap, BBMap, F);
              auto I = std::make_unique<IRInstruction>(Opcode::ICmp, TCtx.getInt1Ty(), 0, dialect::DialectID::Core, InstName);
              if (L) I->addOperand(L);
              if (R) I->addOperand(R);
              Inst = BB->push_back(std::move(I));
              break;
            }
```

修改后：
```cpp
            case Opcode::ICmp: {
              // icmp <pred> <type> <lhs>, <rhs>
              auto PredStr = readIdent();
              auto Pred = textToICmpPred(PredStr);
              skipWS();
              auto* Ty = parseType();
              skipWS();
              auto [L, _] = parseValueRef(ValMap, BBMap, F);
              skipWS();
              if (peek() == ',') advance();
              skipWS();
              auto [R, __] = parseValueRef(ValMap, BBMap, F);
              auto I = std::make_unique<IRInstruction>(Opcode::ICmp, TCtx.getInt1Ty(), 0, dialect::DialectID::Core, InstName);
              I->setPredicate(static_cast<uint8_t>(Pred));
              if (L) I->addOperand(L);
              if (R) I->addOperand(R);
              Inst = BB->push_back(std::move(I));
              break;
            }
```

需要新增 `textToICmpPred` 和 `textToFCmpPred` 辅助函数（在 IRSerializer.cpp 的匿名命名空间中）：

```cpp
static ICmpPred textToICmpPred(StringRef S) {
  if (S == "eq")  return ICmpPred::EQ;
  if (S == "ne")  return ICmpPred::NE;
  if (S == "ugt") return ICmpPred::UGT;
  if (S == "uge") return ICmpPred::UGE;
  if (S == "ult") return ICmpPred::ULT;
  if (S == "ule") return ICmpPred::ULE;
  if (S == "sgt") return ICmpPred::SGT;
  if (S == "sge") return ICmpPred::SGE;
  if (S == "slt") return ICmpPred::SLT;
  if (S == "sle") return ICmpPred::SLE;
  return ICmpPred::EQ; // fallback
}

static FCmpPred textToFCmpPred(StringRef S) {
  if (S == "false") return FCmpPred::False;
  if (S == "oeq")   return FCmpPred::OEQ;
  if (S == "ogt")   return FCmpPred::OGT;
  if (S == "oge")   return FCmpPred::OGE;
  if (S == "olt")   return FCmpPred::OLT;
  if (S == "ole")   return FCmpPred::OLE;
  if (S == "one")   return FCmpPred::ONE;
  if (S == "ord")   return FCmpPred::ORD;
  if (S == "uno")   return FCmpPred::UNO;
  if (S == "ueq")   return FCmpPred::UEQ;
  if (S == "ugt")   return FCmpPred::UGT;
  if (S == "uge")   return FCmpPred::UGE;
  if (S == "ult")   return FCmpPred::ULT;
  if (S == "ule")   return FCmpPred::ULE;
  if (S == "une")   return FCmpPred::UNE;
  if (S == "true")  return FCmpPred::True;
  return FCmpPred::False; // fallback
}
```

#### 4c. parseText 中添加 FCmp case（如果当前 FCmp 解析缺少类似 ICmp 的代码块）

检查文本解析器中是否已有 FCmp case。如果有，按照 ICmp 相同模式修改：读取 PredStr → 转换 → setPredicate。

#### 4d. 二进制序列化（writeBitcode/parseBitcode）

在二进制写入每条指令时，如果是 ICmp/FCmp，写入额外的 1 字节 predicate：
```cpp
// 写入时
if (Op == Opcode::ICmp || Op == Opcode::FCmp) {
  writeuint8(I.getPredicate());
}
```

在二进制读取时，恢复 predicate：
```cpp
// 读取时
if (Op == Opcode::ICmp || Op == Opcode::FCmp) {
  I->setPredicate(readUint8());
}
```

> 注：具体写入位置应在指令操作码之后、操作数之前，以保持编码一致性。dev-runner 需要查看当前二进制编码的具体实现来确定精确插入点。

---

### 修改 5：IRVerifier.cpp — 验证 predicate 合法性

**文件**：`src/IR/IRVerifier.cpp`

在 `verifyCmpOp` 方法（约第 528~553 行）的末尾添加：

```cpp
  // 验证 predicate 值合法
  if (Op == Opcode::ICmp) {
    auto P = I.getICmpPredicate();
    auto V = static_cast<uint8_t>(P);
    if (V > 9) {  // ICmpPred 范围: 0~9 (EQ~SLE)
      reportError(VerificationCategory::InstructionLevel,
                  "ICmp predicate value " + std::to_string(V) + " out of range [0, 9]");
    }
  } else { // FCmp
    auto P = I.getFCmpPredicate();
    auto V = static_cast<uint8_t>(P);
    if (V > 15) {  // FCmpPred 范围: 0~15 (False~True)
      reportError(VerificationCategory::InstructionLevel,
                  "FCmp predicate value " + std::to_string(V) + " out of range [0, 15]");
    }
  }
```

---

## 测试修改规范

### tests/unit/IR/IRBuilderTest.cpp — 新增测试

在现有 ICmp/FCmp 测试（约第 148~158 行）之后添加：

```cpp
TEST_F(IRBuilderTest, ICmpPredicateStorage) {
  auto* A = Builder.getInt32(1);
  auto* B = Builder.getInt32(2);
  auto* ICmpSGT = Builder.createICmp(ICmpPred::SGT, A, B, "sgt");
  EXPECT_EQ(ICmpSGT->getICmpPredicate(), ICmpPred::SGT);
  EXPECT_EQ(ICmpSGT->getPredicate(), static_cast<uint8_t>(ICmpPred::SGT));

  auto* ICmpULE = Builder.createICmp(ICmpPred::ULE, A, B, "ule");
  EXPECT_EQ(ICmpULE->getICmpPredicate(), ICmpPred::ULE);
}

TEST_F(IRBuilderTest, FCmpPredicateStorage) {
  auto* A = Builder.getInt32(1);
  auto* B = Builder.getInt32(2);
  auto* FCmpUNO = Builder.createFCmp(FCmpPred::UNO, A, B, "uno");
  EXPECT_EQ(FCmpUNO->getFCmpPredicate(), FCmpPred::UNO);

  auto* FCmpTrue = Builder.createFCmp(FCmpPred::True, A, B, "always");
  EXPECT_EQ(FCmpTrue->getFCmpPredicate(), FCmpPred::True);
}
```

### tests/unit/IR/IRSerializerTest.cpp — 新增测试

```cpp
TEST_F(IRSerializerTest, ICmpPredicateRoundTrip) {
  // 构建含 icmp sgt 的模块
  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty(), TCtx.getInt32Ty()});
  auto* F = Module->getOrInsertFunction("cmp_sgt", FTy);
  auto* Entry = F->addBasicBlock("entry");
  Builder.setInsertPoint(Entry);
  auto* A = Builder.getInt32(1);
  auto* B = Builder.getInt32(2);
  auto* ICmp = Builder.createICmp(ICmpPred::SGT, A, B, "cmp");
  Builder.createRet(ICmp);

  // 序列化
  std::string Text;
  raw_string_ostream OS(Text);
  IRWriter::writeText(*Module, OS);

  // 验证文本包含 "icmp sgt"
  EXPECT_NE(Text.find("icmp sgt"), std::string::npos);

  // 反序列化
  auto Parsed = IRReader::parseText(StringRef(Text), TCtx);
  ASSERT_NE(Parsed, nullptr);

  // 查找解析后的 icmp 指令并验证 predicate
  auto* PF = Parsed->getFunction("cmp_sgt");
  ASSERT_NE(PF, nullptr);
  auto* PEntry = PF->getEntryBlock();
  ASSERT_NE(PEntry, nullptr);
  // 遍历指令找到 ICmp
  for (auto& I : PEntry->getInstList()) {
    if (I->getOpcode() == Opcode::ICmp) {
      EXPECT_EQ(I->getICmpPredicate(), ICmpPred::SGT);
      break;
    }
  }
}

TEST_F(IRSerializerTest, FCmpPredicateRoundTrip) {
  // 同 ICmp 模式，测试 fcmp uno round-trip
  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty(), TCtx.getInt32Ty()});
  auto* F = Module->getOrInsertFunction("fcmp_uno", FTy);
  auto* Entry = F->addBasicBlock("entry");
  Builder.setInsertPoint(Entry);
  auto* A = Builder.getInt32(1);
  auto* B = Builder.getInt32(2);
  auto* FCmp = Builder.createFCmp(FCmpPred::UNO, A, B, "cmp");
  Builder.createRet(FCmp);

  std::string Text;
  raw_string_ostream OS(Text);
  IRWriter::writeText(*Module, OS);
  EXPECT_NE(Text.find("fcmp uno"), std::string::npos);

  auto Parsed = IRReader::parseText(StringRef(Text), TCtx);
  ASSERT_NE(Parsed, nullptr);
  auto* PF = Parsed->getFunction("fcmp_uno");
  ASSERT_NE(PF, nullptr);
  for (auto& I : PF->getEntryBlock()->getInstList()) {
    if (I->getOpcode() == Opcode::FCmp) {
      EXPECT_EQ(I->getFCmpPredicate(), FCmpPred::UNO);
      break;
    }
  }
}
```

### tests/unit/IR/IRVerifierTest.cpp — 新增测试

```cpp
TEST_F(IRVerifierTest, InvalidICmpPredicate) {
  auto* FTy = TCtx.getFunctionType(TCtx.getInt32Ty(), {TCtx.getInt32Ty(), TCtx.getInt32Ty()});
  auto* F = Module->getOrInsertFunction("bad_pred", FTy);
  auto* Entry = F->addBasicBlock("entry");
  Builder.setInsertPoint(Entry);
  auto* ICmp = Builder.createICmp(ICmpPred::EQ, Builder.getInt32(1), Builder.getInt32(2), "cmp");
  // 手动设置非法 predicate 值
  ICmp->setPredicate(200);  // 超出 ICmpPred 范围
  Builder.createRet(ICmp);

  SmallVector<VerificationDiagnostic, 32> Errors;
  VerifierPass VP(&Errors);
  bool OK = VP.run(*Module);
  EXPECT_FALSE(OK);
  bool hasPredError = false;
  for (auto& E : Errors) {
    if (E.Message.find("predicate") != std::string::npos) hasPredError = true;
  }
  EXPECT_TRUE(hasPredError);
}
```

---

## CMakeLists.txt 修改

无。不涉及新文件，仅修改现有文件。

---

## 实现约束

1. **最小侵入式**：仅在 `IRInstruction` 内部新增 `uint8_t Pred_` 字段，不改变继承体系
2. **向后兼容**：`Pred_` 默认值 0，所有现有构造函数调用无需修改
3. **构造函数签名不变**：保持 `IRInstruction(Opcode, IRType*, unsigned, DialectID, StringRef)` 不变
4. **不影响非比较指令**：`Pred_` 仅对 ICmp/FCmp 有意义，其他指令忽略该字段
5. **所有现有测试必须通过**：此修改为纯增量，不得破坏任何现有功能
6. **命名空间**：`namespace blocktype::ir`
7. **依赖限制**：不引入新依赖

---

## 验收标准

### V1：ICmp predicate 存储和获取

```cpp
auto* ICmp = Builder.createICmp(ICmpPred::SGT, A, B, "cmp");
ASSERT_NE(ICmp, nullptr);
EXPECT_EQ(ICmp->getICmpPredicate(), ICmpPred::SGT);
EXPECT_EQ(ICmp->getPredicate(), static_cast<uint8_t>(ICmpPred::SGT));
```

### V2：FCmp predicate 存储和获取

```cpp
auto* FCmp = Builder.createFCmp(FCmpPred::UNO, A, B, "cmp");
ASSERT_NE(FCmp, nullptr);
EXPECT_EQ(FCmp->getFCmpPredicate(), FCmpPred::UNO);
```

### V3：ICmp predicate round-trip（文本格式）

```cpp
auto* ICmp = Builder.createICmp(ICmpPred::SGT, A, B, "cmp");
Builder.createRet(ICmp);

std::string Text;
raw_string_ostream OS(Text);
IRWriter::writeText(*Module, OS);
// 文本中必须包含 "icmp sgt"
ASSERT_NE(Text.find("icmp sgt"), std::string::npos);

auto Parsed = IRReader::parseText(StringRef(Text), TCtx);
ASSERT_NE(Parsed, nullptr);
// 解析后的 icmp 指令 predicate 必须是 SGT
// （通过遍历指令验证，具体代码见测试规范）
```

### V4：FCmp predicate round-trip（文本格式）

```cpp
// 同 V3，但测试 FCmpPred::UNO 的 round-trip
```

### V5：所有现有测试通过

```bash
cmake --build build
cd build && ctest --output-on-failure
# 所有测试通过，无回归
```

### V6：Verifier 检测非法 predicate

```cpp
auto* ICmp = Builder.createICmp(ICmpPred::EQ, A, B, "cmp");
ICmp->setPredicate(200);  // 非法值
SmallVector<VerificationDiagnostic, 32> Errors;
VerifierPass VP(&Errors);
bool OK = VP.run(*Module);
EXPECT_FALSE(OK);
```

### V7：向后兼容 — 默认 predicate 值

```cpp
// 未设置 predicate 的 ICmp 指令，默认为 EQ（值 0）
auto I = std::make_unique<IRInstruction>(Opcode::ICmp, TCtx.getInt1Ty(), 0);
EXPECT_EQ(I->getPredicate(), 0);
EXPECT_EQ(I->getICmpPredicate(), ICmpPred::EQ);
```

---

> **Git 提交提醒**：本 Task 完成后，立即执行：
> ```bash
> git add -A && git commit -m "fix(A): 修复 IRInstruction 缺失 ICmpPred/FCmpPred 存储 — 谓词 round-trip 一致性" && git push origin HEAD
> ```
