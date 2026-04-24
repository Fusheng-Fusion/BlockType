#include "blocktype/IR/IRConstant.h"

#include <ostream>

namespace blocktype {
namespace ir {

void IRConstantInt::print(std::ostream& OS) const {
  OS << Value.toString(10, true);
}

void IRConstantFP::print(std::ostream& OS) const {
  OS << Value.toString();
}

void IRConstantNull::print(std::ostream& OS) const {
  OS << "null";
}

void IRConstantUndef::print(std::ostream& OS) const {
  OS << "undef";
}

void IRConstantAggregateZero::print(std::ostream& OS) const {
  OS << "zeroinitializer";
}

void IRConstantStruct::print(std::ostream& OS) const {
  OS << "{ ";
  for (size_t i = 0; i < Elements.size(); ++i) {
    if (i > 0) OS << ", ";
    Elements[i]->print(OS);
  }
  OS << " }";
}

void IRConstantArray::print(std::ostream& OS) const {
  OS << "[ ";
  for (size_t i = 0; i < Elements.size(); ++i) {
    if (i > 0) OS << ", ";
    Elements[i]->print(OS);
  }
  OS << " ]";
}

IRConstantFunctionRef::IRConstantFunctionRef(IRFunction* F)
  : IRConstant(ValueKind::ConstantFunctionRef, nullptr, 0), Func(F) {}

void IRConstantFunctionRef::print(std::ostream& OS) const {
  OS << "@func_ref";
}

IRConstantGlobalRef::IRConstantGlobalRef(IRGlobalVariable* G)
  : IRConstant(ValueKind::ConstantGlobalRef, nullptr, 0), Global(G) {}

void IRConstantGlobalRef::print(std::ostream& OS) const {
  OS << "@global_ref";
}

} // namespace ir
} // namespace blocktype
