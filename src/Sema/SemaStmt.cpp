//===--- SemaStmt.cpp - Semantic Analysis for Statements -*- C++ -*-=======//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements statement-related Sema methods: ActOn*Stmt,
// statement factory methods, and C++ statement extensions.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"

#include "llvm/Support/Casting.h"

namespace blocktype {

//===----------------------------------------------------------------------===//
// Statement handling
//===----------------------------------------------------------------------===//

StmtResult Sema::ActOnReturnStmt(Expr *RetVal, SourceLocation ReturnLoc) {
  // Check return value type against current function return type
  if (CurFunction) {
    QualType FnType = CurFunction->getType();
    if (auto *FT = llvm::dyn_cast<FunctionType>(FnType.getTypePtr())) {
      QualType RetType = QualType(FT->getReturnType(), Qualifier::None);
      
      // Skip check if return type is AutoType (will be deduced during instantiation)
      if (RetType.getTypePtr() && RetType->getTypeClass() != TypeClass::Auto) {
        if (!TC.CheckReturn(RetVal, RetType, ReturnLoc))
          return StmtResult::getInvalid();
      }
    }
  }

  auto *RS = Context.create<ReturnStmt>(ReturnLoc, RetVal);
  return StmtResult(RS);
}

StmtResult Sema::ActOnIfStmt(Expr *Cond, Stmt *Then, Stmt *Else,
                              SourceLocation IfLoc,
                              VarDecl *CondVar, bool IsConsteval,
                              bool IsNegated) {
  // Defensive: skip condition check if type not yet set (incremental migration)
  if (!IsConsteval && Cond && !Cond->getType().isNull()
      && !TC.CheckCondition(Cond, IfLoc))
    return StmtResult::getInvalid();

  auto *IS = Context.create<IfStmt>(IfLoc, Cond, Then, Else, CondVar,
                                     IsConsteval, IsNegated);
  return StmtResult(IS);
}

/// P0963R3: ActOnIfStmt with structured bindings
/// Syntax: if (auto [x, y] = expr) { ... }
StmtResult Sema::ActOnIfStmtWithBindings(Expr *Cond, Stmt *Then, Stmt *Else,
                                         SourceLocation IfLoc,
                                         llvm::ArrayRef<class BindingDecl *> Bindings,
                                         bool IsConsteval, bool IsNegated) {
  // Check condition type
  if (!IsConsteval && Cond && !Cond->getType().isNull()
      && !TC.CheckCondition(Cond, IfLoc))
    return StmtResult::getInvalid();
  
  // Validate bindings
  if (Bindings.empty()) {
    Diags.report(IfLoc, DiagID::err_expected_expression);
    return StmtResult::getInvalid();
  }

  // P0963R3: Check binding condition (contextually convertible to bool)
  if (!CheckBindingCondition(Bindings, IfLoc)) {
    return StmtResult::getInvalid();
  }

  // Create IfStmt with structured bindings
  auto *IS = Context.create<IfStmt>(IfLoc, Cond, Then, Else, Bindings,
                                     IsConsteval, IsNegated);
  return StmtResult(IS);
}

StmtResult Sema::ActOnWhileStmt(Expr *Cond, Stmt *Body,
                                 SourceLocation WhileLoc,
                                 VarDecl *CondVar) {
  ++BreakScopeDepth;
  ++ContinueScopeDepth;
  if (Cond && !Cond->getType().isNull() && !TC.CheckCondition(Cond, WhileLoc)) {
    --BreakScopeDepth;
    --ContinueScopeDepth;
    return StmtResult::getInvalid();
  }

  auto *WS = Context.create<WhileStmt>(WhileLoc, Cond, Body, CondVar);
  --BreakScopeDepth;
  --ContinueScopeDepth;
  return StmtResult(WS);
}

StmtResult Sema::ActOnWhileStmtWithBindings(Expr *Cond, Stmt *Body,
                                             SourceLocation WhileLoc,
                                             llvm::ArrayRef<class BindingDecl *> Bindings) {
  ++BreakScopeDepth;
  ++ContinueScopeDepth;
  if (Cond && !Cond->getType().isNull() && !TC.CheckCondition(Cond, WhileLoc)) {
    --BreakScopeDepth;
    --ContinueScopeDepth;
    return StmtResult::getInvalid();
  }

  // P0963R3: Check binding condition (contextually convertible to bool)
  if (!CheckBindingCondition(Bindings, WhileLoc)) {
    --BreakScopeDepth;
    --ContinueScopeDepth;
    return StmtResult::getInvalid();
  }

  auto *WS = Context.create<WhileStmt>(WhileLoc, Cond, Body, nullptr);
  WS->setBindingDecls(Bindings);
  --BreakScopeDepth;
  --ContinueScopeDepth;
  return StmtResult(WS);
}

StmtResult Sema::ActOnForStmt(Stmt *Init, Expr *Cond, Expr *Inc, Stmt *Body,
                               SourceLocation ForLoc) {
  ++BreakScopeDepth;
  ++ContinueScopeDepth;
  if (Cond && !Cond->getType().isNull() && !TC.CheckCondition(Cond, ForLoc)) {
    --BreakScopeDepth;
    --ContinueScopeDepth;
    return StmtResult::getInvalid();
  }

  auto *FS = Context.create<ForStmt>(ForLoc, Init, Cond, Inc, Body);
  --BreakScopeDepth;
  --ContinueScopeDepth;
  return StmtResult(FS);
}

StmtResult Sema::ActOnDoStmt(Expr *Cond, Stmt *Body, SourceLocation DoLoc) {
  ++BreakScopeDepth;
  ++ContinueScopeDepth;
  if (Cond && !Cond->getType().isNull() && !TC.CheckCondition(Cond, DoLoc)) {
    --BreakScopeDepth;
    --ContinueScopeDepth;
    return StmtResult::getInvalid();
  }

  auto *DS = Context.create<DoStmt>(DoLoc, Body, Cond);
  --BreakScopeDepth;
  --ContinueScopeDepth;
  return StmtResult(DS);
}

StmtResult Sema::ActOnSwitchStmt(Expr *Cond, Stmt *Body,
                                  SourceLocation SwitchLoc,
                                  VarDecl *CondVar) {
  ++BreakScopeDepth;
  ++SwitchScopeDepth;
  // Defensive: skip type check if type not yet set (incremental migration)
  if (Cond && !Cond->getType().isNull() && !Cond->getType()->isIntegerType()) {
    Diags.report(SwitchLoc, DiagID::err_condition_not_bool);
    --BreakScopeDepth;
    --SwitchScopeDepth;
    return StmtResult::getInvalid();
  }

  auto *SS = Context.create<SwitchStmt>(SwitchLoc, Cond, Body, CondVar);
  --BreakScopeDepth;
  --SwitchScopeDepth;
  return StmtResult(SS);
}

StmtResult Sema::ActOnCaseStmt(Expr *Val, Expr *RHS, Stmt *Body,
                               SourceLocation CaseLoc) {
  if (SwitchScopeDepth == 0) {
    Diags.report(CaseLoc, DiagID::err_case_not_in_switch);
    // Fall through: still create the node for error recovery
  }
  if (Val && !Val->getType().isNull() && !TC.CheckCaseExpression(Val, CaseLoc))
    return StmtResult::getInvalid();

  // Validate GNU case range RHS
  if (RHS && !RHS->getType().isNull() && !TC.CheckCaseExpression(RHS, CaseLoc))
    return StmtResult::getInvalid();

  auto *CS = Context.create<CaseStmt>(CaseLoc, Val, RHS, Body);
  return StmtResult(CS);
}

StmtResult Sema::ActOnDefaultStmt(Stmt *Body, SourceLocation DefaultLoc) {
  if (SwitchScopeDepth == 0) {
    Diags.report(DefaultLoc, DiagID::err_case_not_in_switch);
    // Fall through: still create the node for error recovery
  }
  auto *DS = Context.create<DefaultStmt>(DefaultLoc, Body);
  return StmtResult(DS);
}

StmtResult Sema::ActOnBreakStmt(SourceLocation BreakLoc) {
  if (BreakScopeDepth == 0) {
    Diags.report(BreakLoc, DiagID::err_break_outside_loop);
    // Fall through: still create the node for error recovery
  }
  auto *BS = Context.create<BreakStmt>(BreakLoc);
  return StmtResult(BS);
}

StmtResult Sema::ActOnContinueStmt(SourceLocation ContinueLoc) {
  if (ContinueScopeDepth == 0) {
    Diags.report(ContinueLoc, DiagID::err_continue_outside_loop);
    // Fall through: still create the node for error recovery
  }
  auto *CS = Context.create<ContinueStmt>(ContinueLoc);
  return StmtResult(CS);
}

StmtResult Sema::ActOnGotoStmt(llvm::StringRef Label, SourceLocation GotoLoc) {
  // TODO: resolve label to an actual LabelDecl
  auto *LD = Context.create<LabelDecl>(GotoLoc, Label);
  auto *GS = Context.create<GotoStmt>(GotoLoc, LD);
  return StmtResult(GS);
}

StmtResult Sema::ActOnCompoundStmt(llvm::ArrayRef<Stmt *> Stmts,
                                    SourceLocation LBraceLoc,
                                    SourceLocation RBraceLoc) {
  auto *CS = Context.create<CompoundStmt>(LBraceLoc, Stmts);
  return StmtResult(CS);
}

StmtResult Sema::ActOnNullStmt(SourceLocation Loc) {
  auto *NS = Context.create<NullStmt>(Loc);
  return StmtResult(NS);
}

//===----------------------------------------------------------------------===//
// Label and expression statements (Phase 2B)
//===----------------------------------------------------------------------===//

StmtResult Sema::ActOnExprStmt(SourceLocation Loc, Expr *E) {
  auto *ES = Context.create<ExprStmt>(Loc, E);
  return StmtResult(ES);
}

StmtResult Sema::ActOnLabelStmt(SourceLocation Loc, llvm::StringRef LabelName,
                                 Stmt *SubStmt) {
  auto *LD = Context.create<LabelDecl>(Loc, LabelName);
  registerDecl(LD);
  auto *LS = Context.create<LabelStmt>(Loc, LD, SubStmt);
  return StmtResult(LS);
}

//===----------------------------------------------------------------------===//
// C++ statement extensions (Phase 2B)
//===----------------------------------------------------------------------===//

StmtResult Sema::ActOnCXXForRangeStmt(SourceLocation ForLoc,
                                       SourceLocation VarLoc, llvm::StringRef VarName,
                                       QualType VarType, Expr *Range, Stmt *Body) {
  ++BreakScopeDepth;
  ++ContinueScopeDepth;
  auto *RangeVar = Context.create<VarDecl>(VarLoc, VarName, VarType, nullptr);
  auto *FRS = Context.create<CXXForRangeStmt>(ForLoc, RangeVar, Range, Body);
  --BreakScopeDepth;
  --ContinueScopeDepth;
  return StmtResult(FRS);
}

StmtResult Sema::ActOnCXXTryStmt(SourceLocation TryLoc, Stmt *TryBlock,
                                  llvm::ArrayRef<Stmt *> Handlers) {
  auto *TS = Context.create<CXXTryStmt>(TryLoc, TryBlock, Handlers);
  return StmtResult(TS);
}

StmtResult Sema::ActOnCXXCatchStmt(SourceLocation CatchLoc,
                                    VarDecl *ExceptionDecl,
                                    Stmt *HandlerBlock) {
  auto *CS = Context.create<CXXCatchStmt>(CatchLoc, ExceptionDecl,
                                          HandlerBlock);
  return StmtResult(CS);
}

StmtResult Sema::ActOnCoreturnStmt(SourceLocation Loc, Expr *RetVal) {
  auto *CS = Context.create<CoreturnStmt>(Loc, RetVal);
  return StmtResult(CS);
}

StmtResult Sema::ActOnCoyieldStmt(SourceLocation Loc, Expr *Value) {
  auto *CS = Context.create<CoyieldStmt>(Loc, Value);
  return StmtResult(CS);
}

ExprResult Sema::ActOnCoawaitExpr(SourceLocation Loc, Expr *Operand) {
  auto *CE = Context.create<CoawaitExpr>(Loc, Operand);
  return ExprResult(CE);
}

} // namespace blocktype
