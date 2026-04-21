//===- ModuleVisibility.cpp - Module Visibility Rules -----------*- C++ -*-===//
//
// Part of the BlockType Project, under the BSD 3-Clause License.
// See the LICENSE file in the project root for license information.
//
//===----------------------------------------------------------------------===//
//
// This file implements module visibility rules for C++20 modules.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/Module/ModuleManager.h"
#include "llvm/ADT/SmallVector.h"

namespace blocktype {

//===------------------------------------------------------------------===//
// 模块可见性规则
//===------------------------------------------------------------------===//

/// 检查符号是否在当前模块可见
///
/// 可见性规则：
/// 1. 同一模块内，所有符号可见
/// 2. 不同模块间，只有导出符号可见
/// 3. 必须存在导入关系
/// 4. 支持传递导出（import A; A export import B;）
bool Sema::isSymbolVisible(NamedDecl *D, ModuleDecl *CurrentMod) {
  if (!D || !CurrentMod) {
    return false;
  }

  // TODO: 实现完整的可见性检查
  // 1. 获取符号所属模块
  // ModuleDecl *OwnerMod = D->getOwningModule();

  // 2. 如果符号没有所属模块（全局符号），则可见
  // if (!OwnerMod) {
  //   return true;
  // }

  // 3. 同一模块内，所有符号可见
  // if (OwnerMod == CurrentMod) {
  //   return true;
  // }

  // 简化实现：所有符号都可见
  return true;
}

/// 检查传递导出
///
/// 如果 CurrentMod 导入了 OwnerMod，且 OwnerMod 传递导出了符号，
/// 则符号在 CurrentMod 中可见
bool Sema::checkTransitiveExport(ModuleDecl *CurrentMod, ModuleDecl *OwnerMod) {
  if (!CurrentMod || !OwnerMod) {
    return false;
  }

  // 直接导入
  if (CurrentMod->imports(OwnerMod->getModuleName())) {
    return true;
  }

  // TODO: 实现传递导入检查
  return false;
}

/// 在导入模块中查找符号
NamedDecl *Sema::lookupInImportedModules(llvm::StringRef Name,
                                         ModuleDecl *CurrentMod) {
  if (!CurrentMod) {
    return nullptr;
  }

  // TODO: 实现导入模块符号查找
  return nullptr;
}

/// 检查声明是否在模块接口中导出
bool Sema::isDeclExported(NamedDecl *D) const {
  if (!D) {
    return false;
  }

  // TODO: 检查声明是否在 export 块中
  // if (D->isExported()) {
  //   return true;
  // }

  // TODO: 检查声明是否在模块接口单元中
  return false;
}

/// 获取声明的有效模块
ModuleDecl *Sema::getEffectiveModule(NamedDecl *D) const {
  if (!D) {
    return nullptr;
  }

  // TODO: 实现获取声明所属模块
  // ModuleDecl *OwnerMod = D->getOwningModule();
  // if (OwnerMod) {
  //   return OwnerMod;
  // }

  // 如果声明在全局作用域，检查是否在模块中
  if (ModMgr->isInModule()) {
    return ModMgr->getCurrentModule();
  }

  return nullptr;
}

/// 检查是否可以访问声明
bool Sema::canAccessDecl(NamedDecl *D, Scope *S) {
  if (!D) {
    return false;
  }

  // 如果不在模块中，所有符号都可访问
  if (!ModMgr->isInModule()) {
    return true;
  }

  // 获取当前模块
  ModuleDecl *CurrentMod = ModMgr->getCurrentModule();
  if (!CurrentMod) {
    return true;
  }

  // 检查可见性
  return isSymbolVisible(D, CurrentMod);
}

} // namespace blocktype
