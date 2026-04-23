//===--- SemaCXX.h - C++ Semantic Analysis -------------------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the SemaCXX class for C++ specific semantic analysis.
//
//===----------------------------------------------------------------------===//

#pragma once

#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Stmt.h"

namespace blocktype {

// Forward declarations from Attr.h
enum class ContractKind;
enum class ContractMode;
class ContractAttr;

/// SemaCXX - C++ specific semantic analysis.
///
/// Centralizes semantic checks for C++23/C++26 features including:
/// - Deducing this (P0847R7)
/// - Static operator (P1169R4, P2589R1)
/// - Contracts (P2900R14) - pre-declared
class SemaCXX {
  Sema &S;

public:
  explicit SemaCXX(Sema &SemaRef) : S(SemaRef) {}

  //===------------------------------------------------------------------===//
  // Deducing this (P0847R7)
  //===------------------------------------------------------------------===//

  /// Check the validity of an explicit object parameter.
  ///
  /// Rules (per Clang):
  /// - Only for non-static member functions
  /// - Cannot be used with virtual
  /// - Cannot have default arguments
  /// - Type must be reference or value of class type
  ///
  /// @param FD       Function declaration
  /// @param Param    The explicit object parameter
  /// @param ParamLoc Source location of the parameter
  /// @return true if valid
  bool CheckExplicitObjectParameter(FunctionDecl *FD, ParmVarDecl *Param,
                                    SourceLocation ParamLoc);

  /// Deduce the explicit object parameter type from the call object.
  ///
  /// @param ObjectType The type of the call object
  /// @param ParamType  The declared parameter type
  /// @param VK         The value kind of the call object
  /// @return The deduced type
  QualType DeduceExplicitObjectType(QualType ObjectType, QualType ParamType,
                                    ExprValueKind VK);

  //===------------------------------------------------------------------===//
  // Static operator (P1169R4, P2589R1)
  //===------------------------------------------------------------------===//

  /// Check the validity of a static operator declaration.
  ///
  /// Rules:
  /// - Must be a member function
  /// - Cannot use 'this' pointer
  /// - For static operator(): acts like a free function callable via S::operator()
  /// - For static operator[]: allows multi-parameter subscript
  ///
  /// @param MD  Method declaration
  /// @param Loc Source location
  /// @return true if valid
  bool CheckStaticOperator(CXXMethodDecl *MD, SourceLocation Loc);

  //===------------------------------------------------------------------===//
  // Contracts (P2900R14)
  //===------------------------------------------------------------------===//

  /// Check a contract condition expression for validity.
  ///
  /// Validates that:
  /// - The condition is non-null
  /// - The condition is contextually convertible to bool
  /// - The condition does not have prohibited side effects (warn)
  ///
  /// @param Cond The condition expression
  /// @param Loc  Source location of the contract attribute
  /// @return true if the condition is valid
  bool CheckContractCondition(Expr *Cond, SourceLocation Loc);

  /// Check that a contract is attached to the right kind of declaration.
  ///
  /// - pre/post: only on function declarations
  /// - assert: only in block statements
  ///
  /// @param CA   The contract attribute
  /// @param Ctx  The declaration context (function or block)
  /// @return true if the placement is valid
  bool CheckContractPlacement(ContractAttr *CA, Decl *Ctx);

  /// Build a ContractAttr from parsed components.
  ///
  /// @param Loc  Source location
  /// @param Kind pre/post/assert
  /// @param Cond The condition expression
  /// @return The created ContractAttr, or nullptr on error
  ContractAttr *BuildContractAttr(SourceLocation Loc, ContractKind Kind,
                                  Expr *Cond);

  //===------------------------------------------------------------------===//
  // P7.3.2: Contract inheritance and virtual function interaction
  //===------------------------------------------------------------------===//

  /// Check contract inheritance when a derived class overrides a virtual
  /// function.
  ///
  /// Per P2900R14:
  /// - Derived class preconditions cannot be stricter than base class
  ///   (can be omitted or made more permissive)
  /// - Derived class postconditions cannot be more permissive than base class
  ///   (can be omitted or made more strict)
  ///
  /// This emits diagnostics for violations but does not prevent compilation.
  ///
  /// @param DerivedMD  The overriding method in the derived class
  /// @param BaseMD     The overridden method in the base class
  void CheckContractInheritance(CXXMethodDecl *DerivedMD,
                                CXXMethodDecl *BaseMD);

  /// Inherit contracts from base class methods to the derived class override.
  ///
  /// When a derived class overrides a virtual function, this method collects
  /// the base class's pre/post contracts and adds them to the derived class's
  /// attribute list (if not already present). The derived class's own contracts
  /// are checked against the base class contracts via CheckContractInheritance.
  ///
  /// @param DerivedMD  The overriding method in the derived class
  void InheritBaseContracts(CXXMethodDecl *DerivedMD);

  /// Collect all contracts from a function declaration.
  ///
  /// @param FD  The function declaration
  /// @return Vector of ContractAttr pointers (pre and post only)
  llvm::SmallVector<ContractAttr *, 4> collectContracts(FunctionDecl *FD);

private:
  /// Recursively check a statement tree for CXXThisExpr usage.
  void checkBodyForThisUse(Stmt *Body, SourceLocation Loc);
};

} // namespace blocktype
