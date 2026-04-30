#pragma once
#include "blocktype/Backend/RegisterAllocator.h"

namespace blocktype::backend {

/// 目标 ABI 信息（轻量级，不依赖 CodeGen 层）
struct TargetABIInfo {
  bool IsLittleEndian = true;
  unsigned PointerSize = 8;
  unsigned StackAlignment = 16;
  unsigned MaxVectorAlignment = 16;
};

/// 栈帧降低抽象基类
class FrameLowering {
public:
  virtual ~FrameLowering() = default;

  /// 为函数插入 prologue/epilogue 代码并计算栈帧大小
  virtual void lower(TargetFunction& F,
                     const TargetRegisterInfo& TRI,
                     const TargetABIInfo& ABI) = 0;

  /// 获取函数的栈帧大小
  virtual uint64_t getStackSize(const TargetFunction& F) const = 0;
};

} // namespace blocktype::backend
