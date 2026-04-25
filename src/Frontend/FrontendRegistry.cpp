//===--- FrontendRegistry.cpp - Frontend Registry -----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Frontend/FrontendRegistry.h"

#include <cassert>

namespace blocktype {
namespace frontend {

FrontendRegistry& FrontendRegistry::instance() {
  static FrontendRegistry Inst;
  return Inst;
}

void FrontendRegistry::registerFrontend(ir::StringRef Name,
                                         FrontendFactory Factory) {
  std::string Key = Name.str();
  assert(!Registry_.contains(Key) &&
         "FrontendRegistry: duplicate frontend registration");
  Registry_.insert({std::move(Key), std::move(Factory)});
}

std::unique_ptr<FrontendBase> FrontendRegistry::create(
    ir::StringRef Name,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags) {
  auto It = Registry_.find(Name.str());
  if (It == Registry_.end())
    return nullptr;
  return It->Value(Opts, Diags);
}

std::unique_ptr<FrontendBase> FrontendRegistry::autoSelect(
    ir::StringRef Filename,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags) {
  // Extract file extension: find the last '.' in the filename.
  size_t DotPos = Filename.rfind('.');
  if (DotPos == ir::StringRef::npos)
    return nullptr;

  std::string Ext = Filename.slice(DotPos).str();
  auto It = ExtToName_.find(Ext);
  if (It == ExtToName_.end())
    return nullptr;

  return create(It->Value, Opts, Diags);
}

void FrontendRegistry::addExtensionMapping(ir::StringRef Ext,
                                             ir::StringRef FrontendName) {
  ExtToName_.insert({Ext.str(), FrontendName.str()});
}

bool FrontendRegistry::hasFrontend(ir::StringRef Name) const {
  return Registry_.contains(Name.str());
}

ir::SmallVector<std::string, 4> FrontendRegistry::getRegisteredNames() const {
  ir::SmallVector<std::string, 4> Names;
  for (const auto& Entry : Registry_)
    Names.push_back(Entry.Key);
  return Names;
}

} // namespace frontend
} // namespace blocktype
