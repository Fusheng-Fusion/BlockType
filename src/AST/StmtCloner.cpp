//===--- StmtCloner.cpp - Statement Cloner Implementation ------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/StmtCloner.h"
#include "llvm/Support/raw_ostream.h"

namespace blocktype {

Stmt *StmtCloner::Clone(Stmt *OriginalStmt) {
  if (!OriginalStmt) {
    return nullptr;
  }

  switch (OriginalStmt->getKind()) {
  case ASTNode::NodeKind::CompoundStmtKind:
    return CloneCompoundStmt(llvm::cast<CompoundStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::DeclStmtKind:
    return CloneDeclStmt(llvm::cast<DeclStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::ReturnStmtKind:
    return CloneReturnStmt(llvm::cast<ReturnStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::IfStmtKind:
    return CloneIfStmt(llvm::cast<IfStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::WhileStmtKind:
    return CloneWhileStmt(llvm::cast<WhileStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::ForStmtKind:
    return CloneForStmt(llvm::cast<ForStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::BreakStmtKind:
    return CloneBreakStmt(llvm::cast<BreakStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::ContinueStmtKind:
    return CloneContinueStmt(llvm::cast<ContinueStmt>(OriginalStmt));
    
  case ASTNode::NodeKind::ExprStmtKind:
    return CloneExprStmt(llvm::cast<ExprStmt>(OriginalStmt));
    
  default:
    // TODO: Add support for more statement types
    llvm::errs() << "Warning: Unsupported Stmt kind in cloning: "
                 << static_cast<int>(OriginalStmt->getKind()) << "\n";
    return OriginalStmt; // Return original as fallback
  }
}

Expr *StmtCloner::CloneExpr(Expr *OriginalExpr) {
  if (!OriginalExpr) {
    return nullptr;
  }

  // TODO: Implement expression cloning with type substitution
  // This requires ExprVisitor similar to TypeVisitor
  // For now, return original (placeholder)
  return OriginalExpr;
}

Stmt *StmtCloner::CloneCompoundStmt(CompoundStmt *CS) {
  llvm::SmallVector<Stmt *, 8> ClonedBody;
  
  for (Stmt *S : CS->getBody()) {
    Stmt *Cloned = Clone(S);
    if (Cloned) {
      ClonedBody.push_back(Cloned);
    }
  }
  
  // Create new CompoundStmt with cloned body
  return new CompoundStmt(CS->getLocation(), ClonedBody);
}

Stmt *StmtCloner::CloneDeclStmt(DeclStmt *DS) {
  // TODO: Clone declarations with type substitution
  // This is complex because it involves Decl cloning
  return DS; // Placeholder
}

Stmt *StmtCloner::CloneReturnStmt(ReturnStmt *RS) {
  Expr *ClonedExpr = CloneExpr(RS->getRetValue());
  // TODO: Create new ReturnStmt with cloned expression
  return RS; // Placeholder
}

Stmt *StmtCloner::CloneIfStmt(IfStmt *IS) {
  // TODO: Clone condition and branches
  return IS; // Placeholder
}

Stmt *StmtCloner::CloneWhileStmt(WhileStmt *WS) {
  // TODO: Clone condition and body
  return WS; // Placeholder
}

Stmt *StmtCloner::CloneForStmt(ForStmt *FS) {
  // TODO: Clone init, condition, increment, and body
  return FS; // Placeholder
}

Stmt *StmtCloner::CloneBreakStmt(BreakStmt *BS) {
  return new BreakStmt(BS->getLocation());
}

Stmt *StmtCloner::CloneContinueStmt(ContinueStmt *CS) {
  return new ContinueStmt(CS->getLocation());
}

Stmt *StmtCloner::CloneExprStmt(ExprStmt *ES) {
  Expr *ClonedExpr = CloneExpr(ES->getExpr());
  // TODO: Create new ExprStmt with cloned expression
  return ES; // Placeholder
}

} // namespace blocktype
