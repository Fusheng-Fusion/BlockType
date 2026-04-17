//===--- AccessControlTest.cpp - Test member access control ---*- C++ -*-===//
//
// Test cases for member access control (public/protected/private)
//
//===----------------------------------------------------------------------===//

#include "blocktype/AST/Decl.h"
#include "blocktype/AST/Expr.h"
#include "blocktype/AST/Type.h"
#include "blocktype/Basic/Diagnostics.h"
#include "blocktype/Lex/Preprocessor.h"
#include "blocktype/Parse/Parser.h"
#include "gtest/gtest.h"

using namespace blocktype;

class AccessControlTest : public ::testing::Test {
protected:
  void SetUp() override {
    Diags = std::make_unique<DiagnosticsEngine>();
    PP = std::make_unique<Preprocessor>(*Diags);
    Context = std::make_unique<ASTContext>();
    Parser = std::make_unique<::blocktype::Parser>(*PP, *Context);
  }

  std::unique_ptr<DiagnosticsEngine> Diags;
  std::unique_ptr<Preprocessor> PP;
  std::unique_ptr<ASTContext> Context;
  std::unique_ptr<::blocktype::Parser> Parser;
};

// Test that isDerivedFrom correctly identifies inheritance relationships
TEST_F(AccessControlTest, IsDerivedFrom) {
  // Create base class
  auto *BaseClass = Context->create<CXXRecordDecl>(SourceLocation(), "Base");

  // Create derived class
  auto *DerivedClass = Context->create<CXXRecordDecl>(SourceLocation(), "Derived");

  // Add BaseClass as a base of DerivedClass
  QualType BaseTy = Context->getRecordType(BaseClass);
  CXXRecordDecl::BaseSpecifier BaseSpec(BaseTy, SourceLocation(), false, true, 2); // public
  DerivedClass->addBase(BaseSpec);

  // Test inheritance check
  EXPECT_TRUE(DerivedClass->isDerivedFrom(BaseClass))
      << "Derived should be derived from Base";
  EXPECT_FALSE(BaseClass->isDerivedFrom(DerivedClass))
      << "Base should not be derived from Derived";
  EXPECT_FALSE(DerivedClass->isDerivedFrom(DerivedClass))
      << "Class should not be derived from itself";
}

// Test multi-level inheritance
TEST_F(AccessControlTest, MultiLevelInheritance) {
  // Create A <- B <- C inheritance chain
  auto *ClassA = Context->create<CXXRecordDecl>(SourceLocation(), "A");
  auto *ClassB = Context->create<CXXRecordDecl>(SourceLocation(), "B");
  auto *ClassC = Context->create<CXXRecordDecl>(SourceLocation(), "C");

  // B inherits from A
  QualType TypeA = Context->getRecordType(ClassA);
  CXXRecordDecl::BaseSpecifier BaseA(TypeA, SourceLocation(), false, true, 2);
  ClassB->addBase(BaseA);

  // C inherits from B
  QualType TypeB = Context->getRecordType(ClassB);
  CXXRecordDecl::BaseSpecifier BaseB(TypeB, SourceLocation(), false, true, 2);
  ClassC->addBase(BaseB);

  // Test multi-level inheritance
  EXPECT_TRUE(ClassC->isDerivedFrom(ClassB)) << "C should derive from B";
  EXPECT_TRUE(ClassC->isDerivedFrom(ClassA)) << "C should derive from A (indirectly)";
  EXPECT_TRUE(ClassB->isDerivedFrom(ClassA)) << "B should derive from A";
  EXPECT_FALSE(ClassA->isDerivedFrom(ClassB)) << "A should not derive from B";
  EXPECT_FALSE(ClassA->isDerivedFrom(ClassC)) << "A should not derive from C";
}

// Test that FieldDecl stores access specifier correctly
TEST_F(AccessControlTest, FieldAccessSpecifier) {
  auto *Class = Context->create<CXXRecordDecl>(SourceLocation(), "MyClass");

  // Create fields with different access levels
  auto *PublicField = Context->create<FieldDecl>(
      SourceLocation(), "publicField", QualType(), AccessSpecifier::AS_public);
  auto *ProtectedField = Context->create<FieldDecl>(
      SourceLocation(), "protectedField", QualType(), AccessSpecifier::AS_protected);
  auto *PrivateField = Context->create<FieldDecl>(
      SourceLocation(), "privateField", QualType(), AccessSpecifier::AS_private);

  Class->addMember(PublicField);
  Class->addMember(ProtectedField);
  Class->addMember(PrivateField);

  // Verify access specifiers
  EXPECT_TRUE(PublicField->isPublic()) << "Field should be public";
  EXPECT_FALSE(PublicField->isProtected()) << "Field should not be protected";
  EXPECT_FALSE(PublicField->isPrivate()) << "Field should not be private";

  EXPECT_FALSE(ProtectedField->isPublic()) << "Field should not be public";
  EXPECT_TRUE(ProtectedField->isProtected()) << "Field should be protected";
  EXPECT_FALSE(ProtectedField->isPrivate()) << "Field should not be private";

  EXPECT_FALSE(PrivateField->isPublic()) << "Field should not be public";
  EXPECT_FALSE(PrivateField->isProtected()) << "Field should not be protected";
  EXPECT_TRUE(PrivateField->isPrivate()) << "Field should be private";
}

// Test that CXXMethodDecl stores access specifier correctly
TEST_F(AccessControlTest, MethodAccessSpecifier) {
  auto *Class = Context->create<CXXRecordDecl>(SourceLocation(), "MyClass");

  QualType FuncType = Context->getFunctionProtoType(
      Context->getVoidType(), {}, false);

  auto *PublicMethod = Context->create<CXXMethodDecl>(
      SourceLocation(), "publicMethod", FuncType, Class,
      AccessSpecifier::AS_public);
  auto *ProtectedMethod = Context->create<CXXMethodDecl>(
      SourceLocation(), "protectedMethod", FuncType, Class,
      AccessSpecifier::AS_protected);
  auto *PrivateMethod = Context->create<CXXMethodDecl>(
      SourceLocation(), "privateMethod", FuncType, Class,
      AccessSpecifier::AS_private);

  Class->addMethod(PublicMethod);
  Class->addMethod(ProtectedMethod);
  Class->addMethod(PrivateMethod);

  // Verify access specifiers
  EXPECT_TRUE(PublicMethod->isPublic());
  EXPECT_TRUE(ProtectedMethod->isProtected());
  EXPECT_TRUE(PrivateMethod->isPrivate());
}
