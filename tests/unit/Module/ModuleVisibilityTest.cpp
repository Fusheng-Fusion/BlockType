//===- ModuleVisibilityTest.cpp - Module Visibility Tests -------*- C++ -*-===//
//
// Part of the BlockType Project, under the BSD 3-Clause License.
// See the LICENSE file in the project root for license information.
//
//===----------------------------------------------------------------------===//

#include "blocktype/Sema/Sema.h"
#include "blocktype/AST/ASTContext.h"
#include "blocktype/AST/Decl.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Module/ModuleManager.h"
#include "gtest/gtest.h"

using namespace blocktype;

class ModuleVisibilityTest : public ::testing::Test {
protected:
  ASTContext Context;
  DiagnosticsEngine Diags;
  std::unique_ptr<Sema> SemaPtr;

  void SetUp() override {
    SemaPtr = std::make_unique<Sema>(Context, Diags);
  }
};

TEST_F(ModuleVisibilityTest, ModuleManagerAccess) {
  // 验证 ModuleManager 可访问
  ModuleManager &ModMgr = SemaPtr->getModuleManager();
  EXPECT_FALSE(ModMgr.isInModule());
}

TEST_F(ModuleVisibilityTest, CreateModuleDecl) {
  // 创建模块
  SourceLocation Loc;
  ModuleDecl *MD = llvm::cast<ModuleDecl>(
      SemaPtr->ActOnModuleDecl(Loc, "TestModule", true, "", false, false, false).get());

  // 验证模块创建成功
  EXPECT_NE(MD, nullptr);
  EXPECT_EQ(MD->getModuleName(), "TestModule");
  EXPECT_TRUE(MD->isExported());
}

TEST_F(ModuleVisibilityTest, SetCurrentModule) {
  // 创建模块
  SourceLocation Loc;
  ModuleDecl *MD = llvm::cast<ModuleDecl>(
      SemaPtr->ActOnModuleDecl(Loc, "TestModule", true, "", false, false, false).get());

  // 设置当前模块
  SemaPtr->getModuleManager().setCurrentModule(MD);

  // 验证当前模块设置成功
  EXPECT_TRUE(SemaPtr->getModuleManager().isInModule());
  EXPECT_EQ(SemaPtr->getModuleManager().getCurrentModule(), MD);
}

TEST_F(ModuleVisibilityTest, IsDeclExported) {
  // 创建模块
  SourceLocation Loc;
  ModuleDecl *MD = llvm::cast<ModuleDecl>(
      SemaPtr->ActOnModuleDecl(Loc, "TestModule", true, "", false, false, false).get());

  // TODO: 创建导出声明并测试
  // 目前简化测试
  EXPECT_TRUE(MD->isExported());
}

TEST_F(ModuleVisibilityTest, GetEffectiveModule) {
  // 创建模块
  SourceLocation Loc;
  ModuleDecl *MD = llvm::cast<ModuleDecl>(
      SemaPtr->ActOnModuleDecl(Loc, "TestModule", true, "", false, false, false).get());

  // 设置当前模块
  SemaPtr->getModuleManager().setCurrentModule(MD);

  // TODO: 创建声明并测试 getEffectiveModule
  // 目前简化测试
  EXPECT_TRUE(SemaPtr->getModuleManager().isInModule());
}
