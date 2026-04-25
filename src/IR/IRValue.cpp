#include "blocktype/IR/IRValue.h"

#include <algorithm>

#include "blocktype/IR/IRBasicBlock.h"

namespace blocktype {
namespace ir {

Use::Use(IRValue* V, User* O) : Val(V), Owner(O) {
  if (Val) Val->addUse(this);
}

void Use::set(IRValue* V) {
  if (Val == V) return;
  if (Val) Val->removeUse(this);
  Val = V;
  if (Val) Val->addUse(this);
}

void IRValue::replaceAllUsesWith(IRValue* New) {
  for (auto* U : UseList) {
    U->Val = New;
    if (New) New->addUse(U);
  }
  UseList.clear();
}

void IRValue::removeUse(Use* U) {
  auto It = std::find(UseList.begin(), UseList.end(), U);
  if (It != UseList.end()) UseList.erase(It);
}

void User::addOperand(IRValue* V) {
  Operands.emplace_back(V, this);
}

void IRBasicBlockRef::print(raw_ostream& OS) const {
  OS << "label %" << (BB ? BB->getName() : "<null>");
}

} // namespace ir
} // namespace blocktype
