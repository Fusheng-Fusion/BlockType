//===--- FrontendRegistry.h - Frontend Registry -------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the FrontendRegistry class, a global singleton that
// manages frontend registration and automatic frontend selection based
// on file extensions.
//
//===----------------------------------------------------------------------===//

#ifndef BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H
#define BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H

#include <cassert>
#include <memory>
#include <string>

#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Frontend/FrontendBase.h"
#include "blocktype/IR/ADT.h"

namespace blocktype {
namespace frontend {

/// FrontendRegistry - Global singleton for frontend registration and creation.
///
/// Manages a registry of FrontendFactory functions keyed by frontend name,
/// plus an extension-to-frontend-name mapping for automatic frontend selection.
///
/// Thread safety: Registration is expected to occur during initialization
/// (single-threaded). Lookup/creation may be called from any thread after
/// registration is complete (read-only access to internal maps).
class FrontendRegistry {
  /// Maps frontend name (e.g., "cpp") → factory function.
  ir::StringMap<FrontendFactory> Registry_;

  /// Maps file extension (e.g., ".cpp") → frontend name.
  ir::StringMap<std::string> ExtToName_;

  /// Private constructor for singleton pattern.
  FrontendRegistry() = default;

public:
  /// Get the global FrontendRegistry singleton instance.
  static FrontendRegistry& instance();

  // Non-copyable, non-movable.
  FrontendRegistry(const FrontendRegistry&) = delete;
  FrontendRegistry& operator=(const FrontendRegistry&) = delete;
  FrontendRegistry(FrontendRegistry&&) = delete;
  FrontendRegistry& operator=(FrontendRegistry&&) = delete;

  /// Register a frontend factory under the given name.
  ///
  /// \param Name     Frontend name (e.g., "cpp", "bt").
  /// \param Factory  Factory function to create instances of this frontend.
  ///
  /// Precondition: No frontend with the same name is already registered.
  /// Asserts on duplicate registration.
  void registerFrontend(ir::StringRef Name, FrontendFactory Factory);

  /// Create a frontend instance by name.
  ///
  /// \param Name   Registered frontend name.
  /// \param Opts   Compile options for the frontend.
  /// \param Diags  Diagnostics engine for error reporting.
  /// \returns Owning pointer to the created frontend, or nullptr if not found.
  std::unique_ptr<FrontendBase> create(
    ir::StringRef Name,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags);

  /// Automatically select and create a frontend based on file extension.
  ///
  /// Extracts the file extension from Filename, looks up the extension
  /// mapping, and creates the corresponding frontend.
  ///
  /// \param Filename  Source file path (used to extract extension).
  /// \param Opts      Compile options for the frontend.
  /// \param Diags     Diagnostics engine for error reporting.
  /// \returns Owning pointer to the created frontend, or nullptr if no
  ///          matching extension mapping is found.
  std::unique_ptr<FrontendBase> autoSelect(
    ir::StringRef Filename,
    const FrontendCompileOptions& Opts,
    DiagnosticsEngine& Diags);

  /// Add a file extension to frontend name mapping.
  ///
  /// \param Ext           File extension including dot (e.g., ".cpp").
  /// \param FrontendName  Name of the registered frontend.
  void addExtensionMapping(ir::StringRef Ext, ir::StringRef FrontendName);

  /// Check if a frontend with the given name is registered.
  bool hasFrontend(ir::StringRef Name) const;

  /// Get all registered frontend names.
  ir::SmallVector<std::string, 4> getRegisteredNames() const;
};

} // namespace frontend
} // namespace blocktype

#endif // BLOCKTYPE_FRONTEND_FRONTENDREGISTRY_H
