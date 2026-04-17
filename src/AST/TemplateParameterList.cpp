//===--- TemplateParameterList.cpp - Template Parameter List --*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/TemplateParameterList.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

bool TemplateParameterList::hasParameterPack() const {
  return getNumParameterPacks() > 0;
}

unsigned TemplateParameterList::getNumParameterPacks() const {
  unsigned Count = 0;
  for (auto *Param : Params) {
    if (auto *TTP = llvm::dyn_cast<TemplateTypeParmDecl>(Param)) {
      if (TTP->isParameterPack())
        ++Count;
    } else if (auto *NTTP = llvm::dyn_cast<NonTypeTemplateParmDecl>(Param)) {
      if (NTTP->isParameterPack())
        ++Count;
    } else if (auto *TTP = llvm::dyn_cast<TemplateTemplateParmDecl>(Param)) {
      if (TTP->isParameterPack())
        ++Count;
    }
  }
  return Count;
}

unsigned TemplateParameterList::getMinRequiredArguments() const {
  unsigned Required = 0;
  for (auto *Param : Params) {
    bool HasDefault = false;
    bool IsPack = false;

    if (auto *TTP = llvm::dyn_cast<TemplateTypeParmDecl>(Param)) {
      HasDefault = !TTP->getDefaultArgument().isNull();
      IsPack = TTP->isParameterPack();
    } else if (auto *NTTP = llvm::dyn_cast<NonTypeTemplateParmDecl>(Param)) {
      HasDefault = NTTP->hasDefaultArgument();
      IsPack = NTTP->isParameterPack();
    } else if (auto *TTP = llvm::dyn_cast<TemplateTemplateParmDecl>(Param)) {
      HasDefault = TTP->hasDefaultArgument();
      IsPack = TTP->isParameterPack();
    }

    // Parameter packs don't require arguments; params with defaults don't either.
    if (IsPack || HasDefault)
      break;
    ++Required;
  }
  return Required;
}

void TemplateParameterList::dump(raw_ostream &OS, unsigned Indent) const {
  for (unsigned I = 0; I < Indent; ++I)
    OS << "  ";
  OS << "TemplateParameterList";
  if (TemplateLoc.isValid())
    OS << " <template loc=" << TemplateLoc.getRawEncoding() << ">";
  if (LAngleLoc.isValid())
    OS << " < loc=" << LAngleLoc.getRawEncoding() << ">";
  if (RAngleLoc.isValid())
    OS << " > loc=" << RAngleLoc.getRawEncoding() << ">";
  OS << " " << Params.size() << " param(s)\n";

  for (auto *Param : Params) {
    Param->dump(OS, Indent + 2);
  }

  if (RequiresClause) {
    for (unsigned I = 0; I < Indent + 2; ++I)
      OS << "  ";
    OS << "RequiresClause:\n";
    RequiresClause->dump(OS, Indent + 4);
  }
}

} // namespace blocktype
