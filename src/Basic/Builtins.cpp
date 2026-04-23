//===--- Builtins.cpp - Builtin Function Implementation ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/Basic/Builtins.h"
#include "llvm/ADT/StringMap.h"
#include <cstring>

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
  assert(ID != BuiltinID::NotBuiltin && "Cannot get info for NotBuiltin");
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
  default:
    // __builtin_unreachable, __builtin_expect, __builtin_is_constant_evaluated,
    // __builtin_labs, __builtin_llabs, __builtin_strlen 等
    // 不通过 LLVM intrinsic 实现，在 EmitBuiltinCall 中直接处理
    return "";
  }
}

} // namespace blocktype