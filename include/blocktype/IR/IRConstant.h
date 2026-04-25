#ifndef BLOCKTYPE_IR_IRCONSTANT_H
#define BLOCKTYPE_IR_IRCONSTANT_H

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "blocktype/IR/IRType.h"
#include "blocktype/IR/IRValue.h"

namespace blocktype {
namespace ir {

class IRContext;

#include "blocktype/IR/ADT/APInt.h"
#include "blocktype/IR/ADT/APFloat.h"

class IRConstant : public IRValue {
public:
  IRConstant(ValueKind K, IRType* T, unsigned ID) : IRValue(K, T, ID) {}
  static bool classof(const IRValue* V) { return V->isConstant(); }
};

class IRConstantInt : public IRConstant {
  APInt Value;

public:
  IRConstantInt(IRIntegerType* Ty, const APInt& V)
    : IRConstant(ValueKind::ConstantInt, Ty, 0), Value(V) {}
  IRConstantInt(IRIntegerType* Ty, uint64_t V)
    : IRConstant(ValueKind::ConstantInt, Ty, 0), Value(Ty->getBitWidth(), V) {}
  const APInt& getValue() const { return Value; }
  uint64_t getZExtValue() const { return Value.getZExtValue(); }
  int64_t getSExtValue() const { return Value.getSExtValue(); }
  bool isZero() const { return Value.isZero(); }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantInt; }
  void print(raw_ostream& OS) const override;
};

class IRConstantFP : public IRConstant {
  APFloat Value;

public:
  IRConstantFP(IRFloatType* Ty, const APFloat& V)
    : IRConstant(ValueKind::ConstantFloat, Ty, 0), Value(V) {}
  const APFloat& getValue() const { return Value; }
  bool isZero() const { return Value.isZero(); }
  bool isNaN() const { return Value.isNaN(); }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantFloat; }
  void print(raw_ostream& OS) const override;
};

class IRConstantNull : public IRConstant {
public:
  explicit IRConstantNull(IRType* Ty) : IRConstant(ValueKind::ConstantNull, Ty, 0) {}
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantNull; }
  void print(raw_ostream& OS) const override;
};

class IRConstantUndef : public IRConstant {
  friend class IRContext;

public:
  explicit IRConstantUndef(IRType* Ty) : IRConstant(ValueKind::ConstantUndef, Ty, 0) {}
  static IRConstantUndef* get(IRContext& Ctx, IRType* Ty);
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantUndef; }
  void print(raw_ostream& OS) const override;
};

class IRConstantAggregateZero : public IRConstant {
public:
  explicit IRConstantAggregateZero(IRType* Ty) : IRConstant(ValueKind::ConstantAggregateZero, Ty, 0) {}
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantAggregateZero; }
  void print(raw_ostream& OS) const override;
};

class IRConstantStruct : public IRConstant {
  SmallVector<IRConstant*, 16> Elements;

public:
  IRConstantStruct(IRStructType* Ty, SmallVector<IRConstant*, 16> Elems)
    : IRConstant(ValueKind::ConstantStruct, Ty, 0), Elements(std::move(Elems)) {}
  ArrayRef<IRConstant*> getElements() const { return Elements; }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantStruct; }
  void print(raw_ostream& OS) const override;
};

class IRConstantArray : public IRConstant {
  SmallVector<IRConstant*, 16> Elements;

public:
  IRConstantArray(IRArrayType* Ty, SmallVector<IRConstant*, 16> Elems)
    : IRConstant(ValueKind::ConstantArray, Ty, 0), Elements(std::move(Elems)) {}
  ArrayRef<IRConstant*> getElements() const { return Elements; }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantArray; }
  void print(raw_ostream& OS) const override;
};

class IRConstantFunctionRef : public IRConstant {
  IRFunction* Func;

public:
  explicit IRConstantFunctionRef(IRFunction* F);
  IRFunction* getFunction() const { return Func; }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantFunctionRef; }
  void print(raw_ostream& OS) const override;
};

class IRConstantGlobalRef : public IRConstant {
  IRGlobalVariable* Global;

public:
  explicit IRConstantGlobalRef(IRGlobalVariable* G);
  IRGlobalVariable* getGlobal() const { return Global; }
  static bool classof(const IRValue* V) { return V->getValueKind() == ValueKind::ConstantGlobalRef; }
  void print(raw_ostream& OS) const override;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRCONSTANT_H
