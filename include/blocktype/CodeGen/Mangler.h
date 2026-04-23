//===--- Mangler.h - Itanium C++ ABI Name Mangling ----------*- C++ -*-===//
//
// Part of the BlockType Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Mangler class for generating Itanium C++ ABI mangled
// names. Produces names like _Z3fooi, _ZN3Foo3barEi, _ZN3FooC1Ei, etc.
//
// Reference: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangle
//
//===----------------------------------------------------------------------===//

#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "blocktype/AST/Type.h"
#include <string>
#include <optional>

namespace blocktype {

/// DtorVariant — 析构函数的 Itanium ABI 变体。
enum class DtorVariant {
  Complete,  ///< D1: complete object destructor
  Deleting   ///< D0: deleting destructor (calls operator delete after D1)
};

class CodeGenModule;
class FunctionDecl;
class CXXMethodDecl;
class CXXConstructorDecl;
class CXXDestructorDecl;
class CXXRecordDecl;
class VarDecl;
class NamespaceDecl;
class ParmVarDecl;

/// Mangler — Itanium C++ ABI name mangling（参照 Clang ItaniumMangleContext）。
///
/// 职责：
/// 1. 为函数生成唯一符号名（支持重载区分）
/// 2. 为类成员函数生成限定名（ClassName::methodName）
/// 3. 为构造/析构函数生成特殊符号名
/// 4. 为虚函数表生成标准名称（_ZTV...）
/// 5. 为全局变量生成符号名
///
/// 局限（AST 限制）：
/// - FunctionDecl 没有父作用域指针，无法自动获取命名空间限定
/// - 命名空间限定需要 DeclContext 父链支持
class Mangler {
  CodeGenModule &CGM;

  //===------------------------------------------------------------------===//
  // Substitution 压缩（Itanium ABI §5.3.5）
  //===------------------------------------------------------------------===//
  /// 存储 Decl* 或 QualType opaque 指针，基于实体标识而非名称字符串
  llvm::SmallVector<const void *, 16> Substitutions;
  unsigned SubstSeqNo = 0;

  /// 判断实体名称是否应加入 substitution 表（长度>1的名称才有价值压缩）
  bool shouldAddSubstitution(llvm::StringRef Name) const {
    return Name.size() > 1;
  }

  /// 将实体指针加入 substitution 表，返回索引
  unsigned addSubstitution(const void *Entity) {
    unsigned Idx = SubstSeqNo++;
    Substitutions.push_back(Entity);
    return Idx;
  }

  /// 索引 0 → "S_", 索引 1 → "S0_", 索引 37 → "SA_"
  std::string getSubstitutionEncoding(unsigned Idx) const {
    if (Idx == 0) return "S_";
    std::string Enc = "S";
    unsigned V = Idx - 1;
    if (V < 36) {
      Enc += (V < 10) ? ('0' + V) : ('A' + V - 10);
    } else {
      llvm::SmallString<8> Buf;
      while (V > 0) {
        unsigned Rem = V % 36;
        Buf.push_back(Rem < 10 ? ('0' + Rem) : ('A' + Rem - 10));
        V /= 36;
      }
      Enc.append(Buf.rbegin(), Buf.rend());
    }
    Enc += '_';
    return Enc;
  }

  /// 查找实体指针是否已在 substitution 表中，命中则返回编码
  std::optional<std::string> trySubstitution(const void *Entity) const {
    for (unsigned I = 0; I < Substitutions.size(); ++I) {
      if (Substitutions[I] == Entity)
        return getSubstitutionEncoding(I);
    }
    return std::nullopt;
  }

  /// 重置 substitution 状态（每次顶层 mangle 调用时使用）
  void resetSubstitutions() {
    Substitutions.clear();
    SubstSeqNo = 0;
  }

public:
  explicit Mangler(CodeGenModule &M) : CGM(M) {}

  //===------------------------------------------------------------------===//
  // 主要入口
  //===------------------------------------------------------------------===//

  /// 获取函数的 mangled 名称。
  std::string getMangledName(const FunctionDecl *FD);

  /// 获取全局变量的 mangled 名称。
  std::string getMangledName(const VarDecl *VD);

  /// 获取 VTable 名称（如 _ZTV3Foo）。
  std::string getVTableName(const CXXRecordDecl *RD);

  /// 获取 RTTI 名称（如 _ZTI3Foo）。
  std::string getRTTIName(const CXXRecordDecl *RD);

  /// 获取 VTable typeinfo name（如 _ZTS3Foo）。
  std::string getTypeinfoName(const CXXRecordDecl *RD);

  /// 生成指定类的析构函数变体的完整 mangled name。
  /// 用于编译器生成的 D0 (deleting destructor) 包装函数。
  std::string getMangledDtorName(const CXXRecordDecl *RD, DtorVariant Variant);

  //===------------------------------------------------------------------===//
  // Type mangling
  //===------------------------------------------------------------------===//

  /// 编码一个 QualType 为 Itanium mangling 字符串。
  std::string mangleType(QualType T);

  /// 编码一个 const Type* 为 Itanium mangling 字符串。
  std::string mangleType(const Type *T);

private:
  //===------------------------------------------------------------------===//
  // 内部编码方法
  //===------------------------------------------------------------------===//

  /// 编码 builtin 类型。
  void mangleBuiltinType(const BuiltinType *T, std::string &Out);

  /// 编码指针类型。
  void manglePointerType(const PointerType *T, std::string &Out);

  /// 编码引用类型。
  void mangleReferenceType(const ReferenceType *T, std::string &Out);

  /// 编码数组类型。
  void mangleArrayType(const ArrayType *T, std::string &Out);

  /// 编码函数类型。
  void mangleFunctionType(const FunctionType *T, std::string &Out);

  /// 编码 record 类型。
  void mangleRecordType(const RecordType *T, std::string &Out);

  /// 编码 enum 类型。
  void mangleEnumType(const EnumType *T, std::string &Out);

  /// 编码 typedef 类型（解包到底层类型）。
  void mangleTypedefType(const TypedefType *T, std::string &Out);

  /// 编码 elaborated 类型（解包到命名类型）。
  void mangleElaboratedType(const ElaboratedType *T, std::string &Out);

  /// 编码 template specialization 类型。
  void mangleTemplateSpecializationType(const TemplateSpecializationType *T,
                                        std::string &Out);

  /// 编码 QualType（处理 CVR 限定符）。
  void mangleQualType(QualType QT, std::string &Out);

  /// 编码函数参数类型列表。
  void mangleFunctionParamTypes(llvm::ArrayRef<ParmVarDecl *> Params,
                                std::string &Out);

  /// 编码嵌套名称限定符（namespace::Class::）。
  void mangleNestedName(const CXXRecordDecl *RD, std::string &Out);

  /// 检查一个 CXXRecordDecl 是否位于命名空间内（通过 DeclContext 父链）。
  static bool hasNamespaceParent(const CXXRecordDecl *RD);

  /// 编码 source-name（长度 + 名称）。
  static void mangleSourceName(llvm::StringRef Name, std::string &Out);

  /// 编码构造函数名称。
  void mangleCtorName(const CXXConstructorDecl *Ctor, std::string &Out);

  /// 编码析构函数名称。
  /// \param Variant 选择 D0 (deleting) 或 D1 (complete) 变体，默认 D1
  void mangleDtorName(const CXXDestructorDecl *Dtor, std::string &Out,
                      DtorVariant Variant = DtorVariant::Complete);
};

} // namespace blocktype
