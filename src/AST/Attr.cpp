//===--- Attr.cpp - Attribute AST Node Implementation -------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/Attr.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

void ContractAttr::dump(raw_ostream &OS, unsigned Indent) const {
  for (unsigned I = 0; I < Indent; ++I)
    OS << "  ";
  OS << "ContractAttr [" << getContractKindName(Kind) << ": ";
  if (Condition) {
    OS << "<expr>";
  } else {
    OS << "<<null>>";
  }
  OS << "] mode=" << getContractModeName(Mode) << "\n";
  if (Condition)
    Condition->dump(OS, Indent + 1);
}

} // namespace blocktype
