//===--- CodeGenStmt.cpp - Statement Code Generation ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/CodeGen/CodeGenFunction.h"
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/CodeGen/CodeGenTypes.h"
#include "blocktype/CodeGen/CodeGenConstant.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Type.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Casting.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// CompoundStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitCompoundStmt(CompoundStmt *CompoundStatement) {
  if (!CompoundStatement) {
    return;
  }
  EmitStmts(CompoundStatement->getBody());
}

//===----------------------------------------------------------------------===//
// IfStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitIfStmt(IfStmt *IfStatement) {
  if (!IfStatement) {
    return;
  }

  // 生成条件
  llvm::Value *Condition = EmitExpr(IfStatement->getCond());
  if (!Condition) {
    return;
  }

  llvm::BasicBlock *ThenBB = createBasicBlock("if.then");
  llvm::BasicBlock *ElseBB = createBasicBlock("if.else");
  llvm::BasicBlock *MergeBB = createBasicBlock("if.end");

  Condition = Builder.CreateICmpNE(
      Condition, llvm::Constant::getNullValue(Condition->getType()), "ifcond");
  Builder.CreateCondBr(Condition, ThenBB, ElseBB);

  // Then
  EmitBlock(ThenBB);
  if (IfStatement->getThen()) {
    EmitStmt(IfStatement->getThen());
  }
  if (haveInsertPoint()) {
    Builder.CreateBr(MergeBB);
  }

  // Else
  EmitBlock(ElseBB);
  if (IfStatement->getElse()) {
    EmitStmt(IfStatement->getElse());
  }
  if (haveInsertPoint()) {
    Builder.CreateBr(MergeBB);
  }

  // Merge
  EmitBlock(MergeBB);
}

//===----------------------------------------------------------------------===//
// SwitchStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitSwitchStmt(SwitchStmt *SwitchStatement) {
  if (!SwitchStatement) {
    return;
  }

  llvm::Value *Condition = EmitExpr(SwitchStatement->getCond());
  if (!Condition) {
    return;
  }

  llvm::BasicBlock *DefaultBB = createBasicBlock("switch.default");
  llvm::BasicBlock *EndBB = createBasicBlock("switch.end");

  // Push break target
  BreakContinueStack.push_back({EndBB, nullptr});

  // 遍历 switch body，收集 case 分支并使用 SwitchInst
  // 第一遍：收集 case 值和创建基本块（使用 EmitConstant 避免副作用）
  struct CaseInfo {
    llvm::ConstantInt *Value;
    llvm::BasicBlock *BB;
  };
  llvm::SmallVector<CaseInfo, 8> Cases;
  llvm::DenseMap<llvm::ConstantInt *, llvm::BasicBlock *> CaseValueToBB;
  llvm::BasicBlock *FoundDefaultBB = nullptr;

  auto *Body = SwitchStatement->getBody();
  if (auto *CS = llvm::dyn_cast<CompoundStmt>(Body)) {
    for (Stmt *S : CS->getBody()) {
      if (auto *CaseS = llvm::dyn_cast<CaseStmt>(S)) {
        if (auto *CaseExpr = CaseS->getLHS()) {
          // 使用常量发射而非 EmitExpr 避免副作用
          llvm::Constant *ConstVal = CGM.getConstants().EmitConstant(CaseExpr);
          if (auto *CI = llvm::dyn_cast_or_null<llvm::ConstantInt>(ConstVal)) {
            llvm::BasicBlock *CaseBB = createBasicBlock("switch.case");
            Cases.push_back({CI, CaseBB});
            CaseValueToBB[CI] = CaseBB;
          }
        }
      } else if (auto *DefaultS = llvm::dyn_cast<DefaultStmt>(S)) {
        FoundDefaultBB = DefaultBB;
      }
    }
  }

  // 创建 SwitchInst
  auto *SwitchInst = Builder.CreateSwitch(Condition, DefaultBB, Cases.size());
  for (auto &C : Cases) {
    SwitchInst->addCase(C.Value, C.BB);
  }

  // 第二遍：生成每个 case/default 的代码
  if (auto *CS = llvm::dyn_cast<CompoundStmt>(Body)) {
    for (Stmt *S : CS->getBody()) {
      if (auto *CaseS = llvm::dyn_cast<CaseStmt>(S)) {
        // 从缓存的映射中查找 BB（不再重复求值 case 表达式）
        llvm::BasicBlock *CaseBB = nullptr;
        if (auto *CaseExpr = CaseS->getLHS()) {
          llvm::Constant *ConstVal = CGM.getConstants().EmitConstant(CaseExpr);
          if (auto *CI = llvm::dyn_cast_or_null<llvm::ConstantInt>(ConstVal)) {
            auto It = CaseValueToBB.find(CI);
            if (It != CaseValueToBB.end()) {
              CaseBB = It->second;
            }
          }
        }
        if (CaseBB) {
          EmitBlock(CaseBB);
        }
        if (CaseS->getSubStmt()) {
          EmitStmt(CaseS->getSubStmt());
        }
        // fallthrough: 不添加 br，允许继续到下一个 case
      } else if (auto *DefaultS = llvm::dyn_cast<DefaultStmt>(S)) {
        EmitBlock(DefaultBB);
        if (DefaultS->getSubStmt()) {
          EmitStmt(DefaultS->getSubStmt());
        }
      } else {
        // 普通 fallthrough 代码
        EmitStmt(S);
      }
    }
  }

  // Default case（如果没有在 body 中遇到 DefaultStmt）
  if (!FoundDefaultBB) {
    EmitBlock(DefaultBB);
  }

  // End
  if (haveInsertPoint()) {
    Builder.CreateBr(EndBB);
  }
  EmitBlock(EndBB);

  BreakContinueStack.pop_back();
}

//===----------------------------------------------------------------------===//
// ForStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitForStmt(ForStmt *ForStatement) {
  if (!ForStatement) {
    return;
  }

  // Init
  if (ForStatement->getInit()) {
    EmitStmt(ForStatement->getInit());
  }

  llvm::BasicBlock *CondBB = createBasicBlock("for.cond");
  llvm::BasicBlock *BodyBB = createBasicBlock("for.body");
  llvm::BasicBlock *IncBB = createBasicBlock("for.inc");
  llvm::BasicBlock *EndBB = createBasicBlock("for.end");

  BreakContinueStack.push_back({EndBB, IncBB});

  // Condition
  EmitBlock(CondBB);
  if (ForStatement->getCond()) {
    llvm::Value *Cond = EmitExpr(ForStatement->getCond());
    if (Cond) {
      Cond = Builder.CreateICmpNE(
          Cond, llvm::Constant::getNullValue(Cond->getType()), "forcond");
      Builder.CreateCondBr(Cond, BodyBB, EndBB);
    } else {
      Builder.CreateBr(BodyBB);
    }
  } else {
    Builder.CreateBr(BodyBB); // 无条件循环
  }

  // Body
  EmitBlock(BodyBB);
  if (ForStatement->getBody()) {
    EmitStmt(ForStatement->getBody());
  }
  if (haveInsertPoint()) {
    Builder.CreateBr(IncBB);
  }

  // Increment
  EmitBlock(IncBB);
  if (ForStatement->getInc()) {
    EmitExpr(ForStatement->getInc());
  }
  Builder.CreateBr(CondBB);

  // End
  EmitBlock(EndBB);

  BreakContinueStack.pop_back();
}

//===----------------------------------------------------------------------===//
// WhileStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitWhileStmt(WhileStmt *WhileStatement) {
  if (!WhileStatement) {
    return;
  }

  llvm::BasicBlock *CondBB = createBasicBlock("while.cond");
  llvm::BasicBlock *BodyBB = createBasicBlock("while.body");
  llvm::BasicBlock *EndBB = createBasicBlock("while.end");

  BreakContinueStack.push_back({EndBB, CondBB});

  // Condition
  EmitBlock(CondBB);
  if (WhileStatement->getCond()) {
    llvm::Value *Cond = EmitExpr(WhileStatement->getCond());
    if (Cond) {
      Cond = Builder.CreateICmpNE(
          Cond, llvm::Constant::getNullValue(Cond->getType()), "whilecond");
      Builder.CreateCondBr(Cond, BodyBB, EndBB);
    } else {
      Builder.CreateBr(EndBB);
    }
  } else {
    Builder.CreateBr(EndBB);
  }

  // Body
  EmitBlock(BodyBB);
  if (WhileStatement->getBody()) {
    EmitStmt(WhileStatement->getBody());
  }
  if (haveInsertPoint()) {
    Builder.CreateBr(CondBB);
  }

  // End
  EmitBlock(EndBB);

  BreakContinueStack.pop_back();
}

//===----------------------------------------------------------------------===//
// DoStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitDoStmt(DoStmt *DoStatement) {
  if (!DoStatement) {
    return;
  }

  llvm::BasicBlock *BodyBB = createBasicBlock("do.body");
  llvm::BasicBlock *CondBB = createBasicBlock("do.cond");
  llvm::BasicBlock *EndBB = createBasicBlock("do.end");

  BreakContinueStack.push_back({EndBB, CondBB});

  // Body
  EmitBlock(BodyBB);
  if (DoStatement->getBody()) {
    EmitStmt(DoStatement->getBody());
  }
  if (haveInsertPoint()) {
    Builder.CreateBr(CondBB);
  }

  // Condition
  EmitBlock(CondBB);
  if (DoStatement->getCond()) {
    llvm::Value *Cond = EmitExpr(DoStatement->getCond());
    if (Cond) {
      Cond = Builder.CreateICmpNE(
          Cond, llvm::Constant::getNullValue(Cond->getType()), "docond");
      Builder.CreateCondBr(Cond, BodyBB, EndBB);
    } else {
      Builder.CreateBr(EndBB);
    }
  } else {
    Builder.CreateBr(EndBB);
  }

  // End
  EmitBlock(EndBB);

  BreakContinueStack.pop_back();
}

//===----------------------------------------------------------------------===//
// ReturnStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitReturnStmt(ReturnStmt *ReturnStatement) {
  if (!ReturnStatement) {
    return;
  }

  if (Expr *ReturnExpr = ReturnStatement->getRetValue()) {
    llvm::Value *ReturnValue = EmitExpr(ReturnExpr);
    if (ReturnValue && this->ReturnValue) {
      Builder.CreateStore(ReturnValue, this->ReturnValue);
    } else if (ReturnValue) {
      Builder.CreateRet(ReturnValue);
      return;
    }
  }

  // 跳到统一的 ReturnBlock
  if (ReturnBlock) {
    Builder.CreateBr(ReturnBlock);
  } else {
    if (haveInsertPoint()) {
      Builder.CreateRetVoid();
    }
  }
}

//===----------------------------------------------------------------------===//
// DeclStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitDeclStmt(DeclStmt *DeclarationStatement) {
  if (!DeclarationStatement) {
    return;
  }

  for (Decl *Declaration : DeclarationStatement->getDecls()) {
    if (auto *VariableDecl = llvm::dyn_cast<VarDecl>(Declaration)) {
      QualType VarType = VariableDecl->getType();
      llvm::AllocaInst *Alloca = CreateAlloca(VarType, VariableDecl->getName());
      if (!Alloca) {
        continue;
      }

      setLocalDecl(VariableDecl, Alloca);

      // 初始化
      if (Expr *Initializer = VariableDecl->getInit()) {
        llvm::Value *InitValue = EmitExpr(Initializer);
        if (InitValue) {
          Builder.CreateStore(InitValue, Alloca);
        }
      } else {
        // 零初始化
        llvm::Type *LLVMType = CGM.getTypes().ConvertType(VarType);
        if (LLVMType) {
          Builder.CreateStore(llvm::Constant::getNullValue(LLVMType), Alloca);
        }
      }
    }
  }
}

//===----------------------------------------------------------------------===//
// BreakStmt / ContinueStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitBreakStmt(BreakStmt *BreakStatement) {
  assert(!BreakContinueStack.empty() && "break outside loop/switch");
  Builder.CreateBr(BreakContinueStack.back().BreakBB);
}

void CodeGenFunction::EmitContinueStmt(ContinueStmt *ContinueStatement) {
  assert(!BreakContinueStack.empty() && "continue outside loop");
  Builder.CreateBr(BreakContinueStack.back().ContinueBB);
}

//===----------------------------------------------------------------------===//
// GotoStmt / LabelStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitGotoStmt(GotoStmt *GotoStatement) {
  // 简化：使用 label 地址跳转
  // TODO: 实现 label → BasicBlock 映射
  if (haveInsertPoint()) {
    // 创建临时跳转目标
    llvm::BasicBlock *TargetBB =
        createBasicBlock("goto.target");
    Builder.CreateBr(TargetBB);
  }
}

void CodeGenFunction::EmitLabelStmt(LabelStmt *LabelStatement) {
  if (!LabelStatement) {
    return;
  }

  llvm::BasicBlock *LabelBB =
      createBasicBlock(LabelStatement->getLabel()
                           ? LabelStatement->getLabel()->getName()
                           : "label");
  if (haveInsertPoint()) {
    Builder.CreateBr(LabelBB);
  }
  EmitBlock(LabelBB);

  if (LabelStatement->getSubStmt()) {
    EmitStmt(LabelStatement->getSubStmt());
  }
}

//===----------------------------------------------------------------------===//
// CXXTryStmt
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitCXXTryStmt(CXXTryStmt *TryStatement) {
  if (!TryStatement) {
    return;
  }

  // 简化：直接生成 try block，忽略异常处理
  if (TryStatement->getTryBlock()) {
    EmitStmt(TryStatement->getTryBlock());
  }

  // 生成 catch blocks
  for (Stmt *CatchBlock : TryStatement->getCatchBlocks()) {
    // 简化：生成 catch block 代码但不连接异常处理
    if (CatchBlock) {
      EmitStmt(CatchBlock);
    }
  }
}
