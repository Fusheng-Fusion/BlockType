//===--- Builtins.h - Builtin Function Query Interface ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Builtins class for querying builtin function info.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/StringRef.h"

namespace blocktype {

/// BuiltinID - Enumeration of all builtin function IDs.
enum class BuiltinID : unsigned {
  NotBuiltin = 0,
#define BUILTIN(ID, TYPE, ATTRS) ID,
#include "blocktype/Basic/Builtins.def"
  FirstTSBuiltin
};

/// BuiltinInfo - Static information about a builtin function.
struct BuiltinInfo {
  const char *Name;
  const char *Type;
  const char *Attrs;
};

/// Builtins - Static class for querying builtin function information.
class Builtins {
public:
  /// Check if the given name refers to a known builtin function.
  static bool isBuiltin(llvm::StringRef Name);

  /// Look up a builtin by name. Returns NotBuiltin if not found.
  static BuiltinID lookup(llvm::StringRef Name);

  /// Get the static info for a builtin ID.
  static const BuiltinInfo &getInfo(BuiltinID ID);

  /// Get the corresponding LLVM intrinsic name for a builtin.
  static llvm::StringRef getLLVMIntrinsicName(BuiltinID ID);

  /// Check if a builtin is marked nothrow.
  static bool isNoThrow(BuiltinID ID);

  /// Check if a builtin is marked const (pure).
  static bool isConst(BuiltinID ID);

  /// Check if a builtin is marked noreturn.
  static bool isNoReturn(BuiltinID ID);

private:
  static const BuiltinInfo BuiltinTable[];
  static constexpr unsigned NumBuiltins =
      static_cast<unsigned>(BuiltinID::FirstTSBuiltin);
};

} // namespace blocktype