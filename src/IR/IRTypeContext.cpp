#include "blocktype/IR/IRTypeContext.h"

#include <cstddef>

namespace blocktype {
namespace ir {

IRTypeContext::IRTypeContext()
  : VoidType(), BoolType(1), NumTypesCreated(0) {
  NumTypesCreated += 2;
}

IRIntegerType* IRTypeContext::getIntType(unsigned BitWidth) {
  auto It = IntTypes.find(BitWidth);
  if (It != IntTypes.end()) return It->second.get();
  auto T = std::make_unique<IRIntegerType>(BitWidth);
  auto* Ptr = T.get();
  IntTypes[BitWidth] = std::move(T);
  ++NumTypesCreated;
  return Ptr;
}

IRFloatType* IRTypeContext::getFloatType(unsigned BitWidth) {
  auto It = FloatTypes.find(BitWidth);
  if (It != FloatTypes.end()) return It->second.get();
  auto T = std::make_unique<IRFloatType>(BitWidth);
  auto* Ptr = T.get();
  FloatTypes[BitWidth] = std::move(T);
  ++NumTypesCreated;
  return Ptr;
}

IRPointerType* IRTypeContext::getPointerType(IRType* Pointee, unsigned AddressSpace) {
  for (auto& P : PointerTypes) {
    if (P->getPointeeType()->equals(Pointee) && P->getAddressSpace() == AddressSpace)
      return P.get();
  }
  auto T = std::make_unique<IRPointerType>(Pointee, AddressSpace);
  auto* Ptr = T.get();
  PointerTypes.push_back(std::move(T));
  ++NumTypesCreated;
  return Ptr;
}

IRArrayType* IRTypeContext::getArrayType(IRType* Element, uint64_t Count) {
  for (auto& A : ArrayTypes) {
    if (A->getElementType()->equals(Element) && A->getNumElements() == Count)
      return A.get();
  }
  auto T = std::make_unique<IRArrayType>(Count, Element);
  auto* Ptr = T.get();
  ArrayTypes.push_back(std::move(T));
  ++NumTypesCreated;
  return Ptr;
}

IRVectorType* IRTypeContext::getVectorType(IRType* Element, unsigned Count) {
  for (auto& V : VectorTypes) {
    if (V->getElementType()->equals(Element) && V->getNumElements() == Count)
      return V.get();
  }
  auto T = std::make_unique<IRVectorType>(Count, Element);
  auto* Ptr = T.get();
  VectorTypes.push_back(std::move(T));
  ++NumTypesCreated;
  return Ptr;
}

IRStructType* IRTypeContext::getStructType(std::string_view Name,
                                            std::vector<IRType*> Elems,
                                            bool Packed) {
  std::string Key(Name);
  auto It = NamedStructTypes.find(Key);
  if (It != NamedStructTypes.end()) return It->second.get();
  auto T = std::make_unique<IRStructType>(Name, std::move(Elems), Packed);
  auto* Ptr = T.get();
  NamedStructTypes[Key] = std::move(T);
  ++NumTypesCreated;
  return Ptr;
}

IRStructType* IRTypeContext::getAnonStructType(std::vector<IRType*> Elems, bool Packed) {
  static unsigned AnonCounter = 0;
  std::string Name = "__anon_struct_" + std::to_string(AnonCounter++);
  return getStructType(Name, std::move(Elems), Packed);
}

bool IRTypeContext::setStructBody(IRStructType* S,
                                   std::vector<IRType*> Elems,
                                   bool Packed) {
  assert(S && "StructType cannot be null");
  S->setBody(std::move(Elems), Packed);
  return true;
}

IRFunctionType* IRTypeContext::getFunctionType(IRType* Ret,
                                                std::vector<IRType*> Params,
                                                bool VarArg) {
  for (auto& F : FunctionTypes) {
    if (!F->getReturnType()->equals(Ret)) continue;
    if (F->isVarArg() != VarArg) continue;
    auto& FP = F->getParamTypes();
    if (FP.size() != Params.size()) continue;
    bool Match = true;
    for (size_t i = 0; i < Params.size(); ++i) {
      if (!FP[i]->equals(Params[i])) { Match = false; break; }
    }
    if (Match) return F.get();
  }
  auto T = std::make_unique<IRFunctionType>(Ret, std::move(Params), VarArg);
  auto* Ptr = T.get();
  FunctionTypes.push_back(std::move(T));
  ++NumTypesCreated;
  return Ptr;
}

IROpaqueType* IRTypeContext::getOpaqueType(std::string_view Name) {
  std::string Key(Name);
  auto It = OpaqueTypes.find(Key);
  if (It != OpaqueTypes.end()) return It->second.get();
  auto T = std::make_unique<IROpaqueType>(Name);
  auto* Ptr = T.get();
  OpaqueTypes[Key] = std::move(T);
  ++NumTypesCreated;
  return Ptr;
}

IRStructType* IRTypeContext::getStructTypeByName(std::string_view Name) const {
  auto It = NamedStructTypes.find(std::string(Name));
  return It != NamedStructTypes.end() ? It->second.get() : nullptr;
}

IROpaqueType* IRTypeContext::getOpaqueTypeByName(std::string_view Name) const {
  auto It = OpaqueTypes.find(std::string(Name));
  return It != OpaqueTypes.end() ? It->second.get() : nullptr;
}

size_t IRTypeContext::getMemoryUsage() const {
  size_t Total = sizeof(IRTypeContext);
  Total += IntTypes.size() * (sizeof(unsigned) + sizeof(std::unique_ptr<IRIntegerType>));
  Total += FloatTypes.size() * (sizeof(unsigned) + sizeof(std::unique_ptr<IRFloatType>));
  Total += PointerTypes.size() * sizeof(std::unique_ptr<IRPointerType>);
  Total += ArrayTypes.size() * sizeof(std::unique_ptr<IRArrayType>);
  Total += VectorTypes.size() * sizeof(std::unique_ptr<IRVectorType>);
  Total += FunctionTypes.size() * sizeof(std::unique_ptr<IRFunctionType>);
  for (auto& P : NamedStructTypes)
    Total += P.first.size() + sizeof(std::unique_ptr<IRStructType>);
  for (auto& P : OpaqueTypes)
    Total += P.first.size() + sizeof(std::unique_ptr<IROpaqueType>);
  return Total;
}

} // namespace ir
} // namespace blocktype
