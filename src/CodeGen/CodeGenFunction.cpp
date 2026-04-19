//===--- CodeGenFunction.cpp - Function-level CodeGen ---------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "blocktype/CodeGen/CodeGenFunction.h"
#include "blocktype/CodeGen/CGCXX.h"
#include "blocktype/CodeGen/CodeGenModule.h"
#include "blocktype/CodeGen/CodeGenTypes.h"
#include "blocktype/CodeGen/CodeGenConstant.h"
#include "blocktype/CodeGen/CGDebugInfo.h"
#include "blocktype/AST/ASTNode.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Stmt.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Type.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Casting.h"

using namespace blocktype;

//===----------------------------------------------------------------------===//
// 构造
//===----------------------------------------------------------------------===//

CodeGenFunction::CodeGenFunction(CodeGenModule &ModuleRef)
    : CGM(ModuleRef), Builder(ModuleRef.getLLVMContext()) {}

//===----------------------------------------------------------------------===//
// 函数体生成
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitFunctionBody(FunctionDecl *FunctionDecl,
                                        llvm::Function *Function) {
  if (!FunctionDecl || !Function) {
    return;
  }

  CurFn = Function;
  CurFD = FunctionDecl;

  // 创建入口块
  llvm::BasicBlock *EntryBlock =
      llvm::BasicBlock::Create(CGM.getLLVMContext(), "entry", Function);
  Builder.SetInsertPoint(EntryBlock);

  // 为返回值创建 alloca（非 void 返回类型）
  QualType ReturnType;
  QualType FDType = FunctionDecl->getType();
  // 获取函数类型的返回类型
  if (!FDType.isNull()) {
    if (auto *FunctionType =
            llvm::dyn_cast<blocktype::FunctionType>(FDType.getTypePtr())) {
      ReturnType =
          QualType(FunctionType->getReturnType(), Qualifier::None);
    }
  }

  // 检测当前函数是否需要 sret（大型结构体通过隐式首参数返回）
  bool FnNeedsSRet = !ReturnType.isNull() && !ReturnType->isVoidType() &&
                     CGM.getTypes().needsSRet(ReturnType);
  IsSRetFn = FnNeedsSRet;

  if (!ReturnType.isNull() && !ReturnType->isVoidType()) {
    ReturnBlock =
        llvm::BasicBlock::Create(CGM.getLLVMContext(), "return", Function);
    if (FnNeedsSRet) {
      // sret 模式：ReturnValue 使用第 0 个参数（隐式 sret 指针）
      // 跳过 sret 隐式首参数后才是真正的用户参数
      auto ArgIt = Function->arg_begin();
      if (ArgIt != Function->arg_end()) {
        llvm::AllocaInst *SRetSave = CreateEntryBlockAlloca(
            llvm::PointerType::get(CGM.getLLVMContext(), 0), "sret.save");
        Builder.CreateStore(&*ArgIt, SRetSave);
        ReturnValue = SRetSave;
      }
    } else {
      ReturnValue = CreateAlloca(ReturnType, "retval");
    }
  }

  // 为函数参数创建 alloca 并拷贝参数值
  // sret 函数：跳过隐式首参数（sret 指针）
  unsigned ArgIndex = 0;
  unsigned ArgSkip = FnNeedsSRet ? 1 : 0;
  for (auto &Arg : Function->args()) {
    if (ArgIndex < ArgSkip) {
      ++ArgIndex;
      continue;
    }
    unsigned ParamIdx = ArgIndex - ArgSkip;
    if (ParamIdx < FunctionDecl->getNumParams()) {
      ParmVarDecl *ParamDecl = FunctionDecl->getParamDecl(ParamIdx);
      llvm::AllocaInst *AllocaInst =
          CreateAlloca(ParamDecl->getType(), ParamDecl->getName());
      Builder.CreateStore(&Arg, AllocaInst);
      setLocalDecl(ParamDecl, AllocaInst);

      // 生成参数调试信息
      if (CGM.getDebugInfo().isInitialized()) {
        CGM.getDebugInfo().EmitParamDI(ParamDecl, AllocaInst, ParamIdx);
      }
    }
    ++ArgIndex;
  }

  // 保存 AllocaInsertPt — 所有后续 alloca 在此位置之前插入
  // 创建一个 dummy alloca 作为锚点，后续 alloca 在其之前插入
  if (AllocaInsertPt) {
    // 已经有 AllocaInsertPt（函数重入），清除旧的
  }
  AllocaInsertPt = Builder.CreateAlloca(
      llvm::Type::getInt32Ty(CGM.getLLVMContext()), nullptr, "alloca.point");
  // AllocaInsertPt 本身就是锚点，后续 alloca 在其之前插入

  // 生成函数体
  if (Stmt *Body = FunctionDecl->getBody()) {
    EmitStmt(Body);
  }

  // 处理 ReturnBlock
  if (ReturnBlock) {
    if (haveInsertPoint()) {
      Builder.CreateBr(ReturnBlock);
    }
    Builder.SetInsertPoint(ReturnBlock);
    if (FnNeedsSRet) {
      // sret 模式：返回 void（结果已写入 sret 指针指向的内存）
      Builder.CreateRetVoid();
    } else {
      llvm::Type *ReturnTy = CGM.getTypes().ConvertType(ReturnType);
      llvm::Value *RetVal = Builder.CreateLoad(ReturnTy, ReturnValue, "ret");
      Builder.CreateRet(RetVal);
    }
  } else {
    // void 函数：确保有终止指令
    if (haveInsertPoint()) {
      Builder.CreateRetVoid();
    }
  }

  // 清除调试信息的函数作用域
  CGM.getDebugInfo().clearCurrentFnSP();
}

//===----------------------------------------------------------------------===//
// 表达式生成
//===----------------------------------------------------------------------===//

llvm::Value *CodeGenFunction::EmitExpr(Expr *Expression) {
  if (!Expression) {
    return nullptr;
  }

  // ExprValueKind 感知：对 glvalue 表达式（lvalue/xvalue），统一走
  // EmitLValue 获取地址 + CreateLoad 读取值，避免各 Emit* 方法重复 load 逻辑。
  // 特例：DeclRefExpr 可能引用函数或枚举常量（prvalue），不走此路径。
  if (Expression->isGLValue()) {
    auto Kind = Expression->getKind();
    // DeclRefExpr 特殊处理：函数引用和枚举常量不是变量地址
    if (Kind == ASTNode::NodeKind::DeclRefExprKind) {
      auto *DRE = llvm::cast<DeclRefExpr>(Expression);
      ValueDecl *D = DRE->getDecl();
      if (!D) return nullptr;
      // 函数引用 → 直接返回函数指针
      if (llvm::isa<FunctionDecl>(D))
        return CGM.GetOrCreateFunctionDecl(llvm::cast<FunctionDecl>(D));
      // 枚举常量 → 直接返回常量值
      if (auto *ECD = llvm::dyn_cast<EnumConstantDecl>(D)) {
        llvm::Type *EnumTy = CGM.getTypes().ConvertType(ECD->getType());
        if (!EnumTy) EnumTy = llvm::Type::getInt32Ty(CGM.getLLVMContext());
        return llvm::ConstantInt::get(EnumTy, ECD->getVal());
      }
      // 变量引用 → EmitLValue + load
      llvm::Value *Addr = EmitLValue(Expression);
      if (!Addr) return nullptr;
      llvm::Type *Ty = CGM.getTypes().ConvertType(Expression->getType());
      return Builder.CreateLoad(Ty, Addr, "lv.load");
    }
    // MemberExpr / ArraySubscriptExpr 等 glvalue → EmitLValue + load
    llvm::Value *Addr = EmitLValue(Expression);
    if (!Addr) return nullptr;
    llvm::Type *Ty = CGM.getTypes().ConvertType(Expression->getType());
    return Builder.CreateLoad(Ty, Addr, "lv.load");
  }

  switch (Expression->getKind()) {
  // 字面量
  case ASTNode::NodeKind::IntegerLiteralKind:
    return CGM.getConstants().EmitIntLiteral(
        llvm::cast<IntegerLiteral>(Expression));
  case ASTNode::NodeKind::FloatingLiteralKind:
    return CGM.getConstants().EmitFloatLiteral(
        llvm::cast<FloatingLiteral>(Expression));
  case ASTNode::NodeKind::StringLiteralKind:
    return CGM.getConstants().EmitStringLiteral(
        llvm::cast<StringLiteral>(Expression));
  case ASTNode::NodeKind::CharacterLiteralKind:
    return CGM.getConstants().EmitCharLiteral(
        llvm::cast<CharacterLiteral>(Expression));
  case ASTNode::NodeKind::CXXBoolLiteralKind:
    return CGM.getConstants().EmitBoolLiteral(
        llvm::cast<CXXBoolLiteral>(Expression)->getValue());
  case ASTNode::NodeKind::CXXNullPtrLiteralKind:
    return CGM.getConstants().EmitNullPointer(Expression->getType());

  // 运算符
  case ASTNode::NodeKind::BinaryOperatorKind:
    return EmitBinaryOperator(llvm::cast<BinaryOperator>(Expression));
  case ASTNode::NodeKind::UnaryOperatorKind:
    return EmitUnaryOperator(llvm::cast<UnaryOperator>(Expression));
  case ASTNode::NodeKind::UnaryExprOrTypeTraitExprKind:
    return EmitUnaryExprOrTypeTraitExpr(
        llvm::cast<UnaryExprOrTypeTraitExpr>(Expression));
  case ASTNode::NodeKind::ConditionalOperatorKind:
    return EmitConditionalOperator(llvm::cast<ConditionalOperator>(Expression));

  // 调用
  case ASTNode::NodeKind::CallExprKind:
  case ASTNode::NodeKind::CXXMemberCallExprKind:
    return EmitCallExpr(llvm::cast<CallExpr>(Expression));

  // C++ 特有
  case ASTNode::NodeKind::CXXThisExprKind:
    return EmitCXXThisExpr(llvm::cast<CXXThisExpr>(Expression));
  case ASTNode::NodeKind::CXXConstructExprKind:
  case ASTNode::NodeKind::CXXTemporaryObjectExprKind:
    return EmitCXXConstructExpr(llvm::cast<CXXConstructExpr>(Expression));
  case ASTNode::NodeKind::CXXNewExprKind:
    return EmitCXXNewExpr(llvm::cast<CXXNewExpr>(Expression));
  case ASTNode::NodeKind::CXXDeleteExprKind:
    return EmitCXXDeleteExpr(llvm::cast<CXXDeleteExpr>(Expression));
  case ASTNode::NodeKind::CXXThrowExprKind:
    return EmitCXXThrowExpr(llvm::cast<CXXThrowExpr>(Expression));

  // 类型转换
  case ASTNode::NodeKind::CXXStaticCastExprKind:
  case ASTNode::NodeKind::CXXDynamicCastExprKind:
  case ASTNode::NodeKind::CXXConstCastExprKind:
  case ASTNode::NodeKind::CXXReinterpretCastExprKind:
  case ASTNode::NodeKind::CStyleCastExprKind:
    return EmitCastExpr(llvm::cast<CastExpr>(Expression));

  // 初始化列表
  case ASTNode::NodeKind::InitListExprKind:
    return EmitInitListExpr(llvm::cast<InitListExpr>(Expression));

  default:
    break;
  }

  return nullptr;
}

//===----------------------------------------------------------------------===//
// 语句生成
//===----------------------------------------------------------------------===//

void CodeGenFunction::EmitStmt(Stmt *Statement) {
  if (!Statement) {
    return;
  }

  switch (Statement->getKind()) {
  case ASTNode::NodeKind::CompoundStmtKind:
    EmitCompoundStmt(llvm::cast<CompoundStmt>(Statement));
    break;
  case ASTNode::NodeKind::ReturnStmtKind:
    EmitReturnStmt(llvm::cast<ReturnStmt>(Statement));
    break;
  case ASTNode::NodeKind::ExprStmtKind: {
    auto *ExpressionStatement = llvm::cast<ExprStmt>(Statement);
    EmitExpr(ExpressionStatement->getExpr());
    break;
  }
  case ASTNode::NodeKind::DeclStmtKind:
    EmitDeclStmt(llvm::cast<DeclStmt>(Statement));
    break;
  case ASTNode::NodeKind::IfStmtKind:
    EmitIfStmt(llvm::cast<IfStmt>(Statement));
    break;
  case ASTNode::NodeKind::SwitchStmtKind:
    EmitSwitchStmt(llvm::cast<SwitchStmt>(Statement));
    break;
  case ASTNode::NodeKind::ForStmtKind:
    EmitForStmt(llvm::cast<ForStmt>(Statement));
    break;
  case ASTNode::NodeKind::WhileStmtKind:
    EmitWhileStmt(llvm::cast<WhileStmt>(Statement));
    break;
  case ASTNode::NodeKind::DoStmtKind:
    EmitDoStmt(llvm::cast<DoStmt>(Statement));
    break;
  case ASTNode::NodeKind::BreakStmtKind:
    EmitBreakStmt(llvm::cast<BreakStmt>(Statement));
    break;
  case ASTNode::NodeKind::ContinueStmtKind:
    EmitContinueStmt(llvm::cast<ContinueStmt>(Statement));
    break;
  case ASTNode::NodeKind::GotoStmtKind:
    EmitGotoStmt(llvm::cast<GotoStmt>(Statement));
    break;
  case ASTNode::NodeKind::LabelStmtKind:
    EmitLabelStmt(llvm::cast<LabelStmt>(Statement));
    break;
  case ASTNode::NodeKind::NullStmtKind:
    // 空语句，什么都不做
    break;
  case ASTNode::NodeKind::CXXTryStmtKind:
    EmitCXXTryStmt(llvm::cast<CXXTryStmt>(Statement));
    break;
  case ASTNode::NodeKind::CXXForRangeStmtKind:
    EmitCXXForRangeStmt(llvm::cast<CXXForRangeStmt>(Statement));
    break;
  case ASTNode::NodeKind::CoreturnStmtKind:
    EmitCoreturnStmt(llvm::cast<CoreturnStmt>(Statement));
    break;
  case ASTNode::NodeKind::CoyieldStmtKind:
    EmitCoyieldStmt(llvm::cast<CoyieldStmt>(Statement));
    break;
  case ASTNode::NodeKind::CaseStmtKind:
  case ASTNode::NodeKind::DefaultStmtKind:
    // CaseStmt/DefaultStmt 由 EmitSwitchStmt 内部处理
    // 如果到达这里说明在 switch 外部，忽略
    break;
  case ASTNode::NodeKind::CXXCatchStmtKind:
    // CXXCatchStmt 由 EmitCXXTryStmt 内部处理
    break;
  default:
    break;
  }
}

void CodeGenFunction::EmitStmts(llvm::ArrayRef<Stmt *> Statements) {
  for (Stmt *Statement : Statements) {
    EmitStmt(Statement);
  }
}

//===----------------------------------------------------------------------===//
// 基本块管理
//===----------------------------------------------------------------------===//

llvm::BasicBlock *CodeGenFunction::createBasicBlock(llvm::StringRef Name) {
  // 不在这里插入 CurFn，由 EmitBlock 负责插入
  return llvm::BasicBlock::Create(CGM.getLLVMContext(), Name);
}

void CodeGenFunction::EmitBlock(llvm::BasicBlock *BasicBlock) {
  // 如果当前块没有终止指令，创建一个分支
  if (haveInsertPoint()) {
    llvm::Instruction *Terminator =
        Builder.GetInsertBlock()->getTerminator();
    if (!Terminator) {
      Builder.CreateBr(BasicBlock);
    }
  }
  CurFn->insert(CurFn->end(), BasicBlock);
  Builder.SetInsertPoint(BasicBlock);
}

//===----------------------------------------------------------------------===//
// 局部变量管理
//===----------------------------------------------------------------------===//

llvm::AllocaInst *CodeGenFunction::CreateAlloca(QualType Type,
                                                  llvm::StringRef Name) {
  llvm::Type *LLVMType = CGM.getTypes().ConvertType(Type);
  if (!LLVMType) {
    return nullptr;
  }
  return CreateEntryBlockAlloca(LLVMType, Name);
}

llvm::AllocaInst *CodeGenFunction::CreateEntryBlockAlloca(llvm::Type *Ty,
                                                            llvm::StringRef Name) {
  if (AllocaInsertPt) {
    // 在 AllocaInsertPt 之前插入
    llvm::IRBuilder<>::InsertPoint SavedIP = Builder.saveIP();
    Builder.SetInsertPoint(AllocaInsertPt);
    llvm::AllocaInst *AllocaInst = Builder.CreateAlloca(Ty, nullptr, Name);
    // 更新 AllocaInsertPt 指向新创建的 alloca（保持插入顺序）
    AllocaInsertPt = AllocaInst;
    Builder.restoreIP(SavedIP);
    return AllocaInst;
  }

  // Fallback: 没有 AllocaInsertPt（函数外或初始化前），在当前位置创建
  return Builder.CreateAlloca(Ty, nullptr, Name);
}

void CodeGenFunction::setLocalDecl(VarDecl *VariableDecl,
                                    llvm::AllocaInst *Alloca) {
  LocalDecls[VariableDecl] = Alloca;
}

llvm::AllocaInst *CodeGenFunction::getLocalDecl(VarDecl *VariableDecl) const {
  auto Iterator = LocalDecls.find(VariableDecl);
  if (Iterator != LocalDecls.end()) {
    return Iterator->second;
  }
  return nullptr;
}

llvm::Value *CodeGenFunction::LoadLocalVar(VarDecl *VariableDecl) {
  llvm::AllocaInst *Alloca = getLocalDecl(VariableDecl);
  if (!Alloca) {
    return nullptr;
  }
  llvm::Type *Type = CGM.getTypes().ConvertType(VariableDecl->getType());
  return Builder.CreateLoad(Type, Alloca, VariableDecl->getName());
}

void CodeGenFunction::StoreLocalVar(VarDecl *VariableDecl,
                                     llvm::Value *Value) {
  llvm::AllocaInst *Alloca = getLocalDecl(VariableDecl);
  if (Alloca && Value) {
    Builder.CreateStore(Value, Alloca);
  }
}

//===----------------------------------------------------------------------===//
// 异常处理辅助
//===----------------------------------------------------------------------===//

llvm::CallBase *CodeGenFunction::EmitCallOrInvoke(
    llvm::FunctionCallee Callee, llvm::ArrayRef<llvm::Value *> Args,
    llvm::StringRef Name) {
  if (isInTryBlock()) {
    const auto &Target = getCurrentInvokeTarget();
    return Builder.CreateInvoke(Callee, Target.NormalBB, Target.UnwindBB,
                                Args, Name);
  }
  return Builder.CreateCall(Callee, Args, Name);
}

llvm::CallBase *CodeGenFunction::EmitNounwindCall(
    llvm::FunctionCallee Callee, llvm::ArrayRef<llvm::Value *> Args,
    llvm::StringRef Name) {
  auto *Call = Builder.CreateCall(Callee, Args, Name);
  Call->setDoesNotThrow();
  return Call;
}

llvm::MDNode *CodeGenFunction::createBranchWeights(llvm::LLVMContext &Ctx,
                                                     unsigned TrueWeight,
                                                     unsigned FalseWeight) {
  llvm::Metadata *Weights[] = {
      llvm::MDString::get(Ctx, "branch_weights"),
      llvm::ConstantAsMetadata::get(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), TrueWeight)),
      llvm::ConstantAsMetadata::get(
          llvm::ConstantInt::get(llvm::Type::getInt32Ty(Ctx), FalseWeight))};
  return llvm::MDNode::get(Ctx, Weights);
}

llvm::MDNode *CodeGenFunction::createIfBranchWeights(llvm::LLVMContext &Ctx,
                                                       const Stmt *ThenStmt,
                                                       bool HasElse) {
  if (!ThenStmt) return nullptr;
  BranchLikelihood BL = ThenStmt->getBranchLikelihood();
  // if HasElse: true branch → ThenBB, false branch → ElseBB
  // if !HasElse: true branch → ThenBB, false branch → MergeBB
  // [[likely]] on then → true weight 高
  // [[unlikely]] on then → false weight 高
  switch (BL) {
  case BranchLikelihood::Likely:
    // then 分支更可能执行
    return createBranchWeights(Ctx, /*TrueWeight=*/2000, /*FalseWeight=*/1);
  case BranchLikelihood::Unlikely:
    // then 分支不太可能执行
    return createBranchWeights(Ctx, /*TrueWeight=*/1, /*FalseWeight=*/2000);
  case BranchLikelihood::None:
    return nullptr;
  }
  return nullptr;
}

llvm::MDNode *CodeGenFunction::createLoopBranchWeights(llvm::LLVMContext &Ctx,
                                                          const Stmt *LoopBody) {
  if (!LoopBody) return nullptr;
  BranchLikelihood BL = LoopBody->getBranchLikelihood();
  // 循环条件: true → BodyBB (继续循环), false → EndBB (退出循环)
  // [[likely]] on body → 继续循环更可能 → true weight 高
  // [[unlikely]] on body → 退出循环更可能 → false weight 高
  switch (BL) {
  case BranchLikelihood::Likely:
    return createBranchWeights(Ctx, /*TrueWeight=*/2000, /*FalseWeight=*/1);
  case BranchLikelihood::Unlikely:
    return createBranchWeights(Ctx, /*TrueWeight=*/1, /*FalseWeight=*/2000);
  case BranchLikelihood::None:
    return nullptr;
  }
  return nullptr;
}

//===----------------------------------------------------------------------===//
// 声明引用表达式
//===----------------------------------------------------------------------===//

llvm::Value *CodeGenFunction::EmitDeclRefExpr(DeclRefExpr *Expression) {
  if (!Expression) {
    return nullptr;
  }

  ValueDecl *Declaration = Expression->getDecl();
  if (!Declaration) {
    return nullptr;
  }

  // 局部变量 / 参数
  if (auto *VariableDecl = llvm::dyn_cast<VarDecl>(Declaration)) {
    if (auto *Alloca = getLocalDecl(VariableDecl)) {
      llvm::Type *Type = CGM.getTypes().ConvertType(VariableDecl->getType());
      return Builder.CreateLoad(Type, Alloca, VariableDecl->getName());
    }
    // 全局变量
    if (auto *GlobalVar = CGM.GetGlobalVar(VariableDecl)) {
      return Builder.CreateLoad(
          CGM.getTypes().ConvertType(VariableDecl->getType()), GlobalVar,
          VariableDecl->getName());
    }
  }

  // 函数引用
  if (auto *FunctionDeclaration =
          llvm::dyn_cast<FunctionDecl>(Declaration)) {
    return CGM.GetOrCreateFunctionDecl(FunctionDeclaration);
  }

  // 枚举常量 — 使用枚举的底层类型而非硬编码 i32
  if (auto *EnumConstant = llvm::dyn_cast<EnumConstantDecl>(Declaration)) {
    llvm::Type *EnumTy = CGM.getTypes().ConvertType(EnumConstant->getType());
    if (!EnumTy) {
      EnumTy = llvm::Type::getInt32Ty(CGM.getLLVMContext());
    }
    return llvm::ConstantInt::get(EnumTy, EnumConstant->getVal());
  }

  return nullptr;
}

//===----------------------------------------------------------------------===//
// Cleanup 栈
//===----------------------------------------------------------------------===//

void CodeGenFunction::pushCleanup(VarDecl *VD) {
  if (!VD)
    return;

  QualType VarType = VD->getType();
  // 检查类型是否是 record 且有非 trivial 析构函数
  if (VarType.isNull() || !VarType->isRecordType())
    return;

  auto *RT = llvm::dyn_cast<RecordType>(VarType.getTypePtr());
  if (!RT)
    return;

  auto *CXXRecord = llvm::dyn_cast<CXXRecordDecl>(RT->getDecl());
  if (!CXXRecord || !CXXRecord->hasDestructor())
    return;

  CleanupStack.push_back({VD, CXXRecord});
}

void CodeGenFunction::EmitCleanupsForScope(unsigned OldSize) {
  // 逆序调用析构函数（后构造的先析构）
  while (CleanupStack.size() > OldSize) {
    CleanupEntry Entry = CleanupStack.pop_back_val();
    llvm::AllocaInst *Alloca = getLocalDecl(Entry.VD);
    if (Alloca && haveInsertPoint()) {
      CGM.getCXX().EmitDestructorCall(*this, Entry.RD, Alloca);
    }
  }
}
