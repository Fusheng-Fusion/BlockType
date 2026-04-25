#include "blocktype/IR/IRContext.h"

#include "blocktype/IR/IRConstant.h"
#include "blocktype/IR/IRModule.h"

namespace blocktype {
namespace ir {

StringRef IRContext::saveString(StringRef Str) {
  size_t Len = Str.size();
  char* Mem = static_cast<char*>(Allocator.Allocate(Len + 1, alignof(char)));
  std::copy(Str.begin(), Str.end(), Mem);
  Mem[Len] = '\0';
  return StringRef(Mem, Len);
}

void IRContext::sealModule(IRModule& M) {
  M.seal();
}

IRConstantUndef* IRContext::getUndef(IRType* Ty) {
  auto It = UndefCache.find(Ty);
  if (It != UndefCache.end()) return (*It).second;
  auto* U = create<IRConstantUndef>(Ty);
  UndefCache[Ty] = U;
  return U;
}

IRConstantUndef* IRConstantUndef::get(IRContext& Ctx, IRType* Ty) {
  return Ctx.getUndef(Ty);
}

} // namespace ir
} // namespace blocktype
