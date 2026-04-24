#ifndef BLOCKTYPE_IR_IRTYPECONTEXT_H
#define BLOCKTYPE_IR_IRTYPECONTEXT_H

#include <cassert>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "blocktype/IR/IRType.h"

namespace blocktype {
namespace ir {

class IRTypeContext {
  std::unordered_map<unsigned, std::unique_ptr<IRIntegerType>> IntTypes;
  std::unordered_map<unsigned, std::unique_ptr<IRFloatType>> FloatTypes;
  std::vector<std::unique_ptr<IRPointerType>> PointerTypes;
  std::vector<std::unique_ptr<IRArrayType>> ArrayTypes;
  std::vector<std::unique_ptr<IRVectorType>> VectorTypes;
  std::unordered_map<std::string, std::unique_ptr<IRStructType>> NamedStructTypes;
  std::unordered_map<std::string, std::unique_ptr<IROpaqueType>> OpaqueTypes;
  std::vector<std::unique_ptr<IRFunctionType>> FunctionTypes;
  IRVoidType VoidType;
  IRIntegerType BoolType;
  unsigned NumTypesCreated = 0;

public:
  IRTypeContext();

  IRVoidType* getVoidType() { return &VoidType; }
  IRIntegerType* getBoolType() { return &BoolType; }
  IRIntegerType* getIntType(unsigned BitWidth);
  IRIntegerType* getInt1Ty()   { return getIntType(1); }
  IRIntegerType* getInt8Ty()   { return getIntType(8); }
  IRIntegerType* getInt16Ty()  { return getIntType(16); }
  IRIntegerType* getInt32Ty()  { return getIntType(32); }
  IRIntegerType* getInt64Ty()  { return getIntType(64); }
  IRIntegerType* getInt128Ty() { return getIntType(128); }
  IRFloatType* getFloatType(unsigned BitWidth);
  IRFloatType* getHalfTy()     { return getFloatType(16); }
  IRFloatType* getFloatTy()    { return getFloatType(32); }
  IRFloatType* getDoubleTy()   { return getFloatType(64); }
  IRFloatType* getFloat80Ty()  { return getFloatType(80); }
  IRFloatType* getFloat128Ty() { return getFloatType(128); }
  IRPointerType* getPointerType(IRType* Pointee, unsigned AddressSpace = 0);
  IRArrayType* getArrayType(IRType* Element, uint64_t Count);
  IRVectorType* getVectorType(IRType* Element, unsigned Count);
  IRStructType* getStructType(std::string_view Name, std::vector<IRType*> Elems, bool Packed = false);
  IRStructType* getAnonStructType(std::vector<IRType*> Elems, bool Packed = false);
  bool setStructBody(IRStructType* S, std::vector<IRType*> Elems, bool Packed = false);
  IRFunctionType* getFunctionType(IRType* Ret, std::vector<IRType*> Params, bool VarArg = false);
  IROpaqueType* getOpaqueType(std::string_view Name);
  IRStructType* getStructTypeByName(std::string_view Name) const;
  IROpaqueType* getOpaqueTypeByName(std::string_view Name) const;
  unsigned getNumTypesCreated() const { return NumTypesCreated; }
  size_t getMemoryUsage() const;
};

} // namespace ir
} // namespace blocktype

#endif // BLOCKTYPE_IR_IRTYPECONTEXT_H
