#pragma once
#include "blocktype/IR/ADT.h"
#include "blocktype/Backend/InstructionSelector.h"  // TargetInstruction
#include <string>
#include <vector>

namespace blocktype::backend {

/// 寄存器分配策略
enum class RegAllocStrategy {
  Greedy = 0,
  Fast   = 1,
  Basic  = 2,
};

// === 前置类型定义 ===

/// 目标基本块
class TargetBasicBlock {
  std::string Name_;
  ir::SmallVector<std::unique_ptr<TargetInstruction>, 16> Instructions_;
public:
  explicit TargetBasicBlock(ir::StringRef Name = "") : Name_(Name.str()) {}
  ir::StringRef getName() const { return Name_; }
  auto& instructions() { return Instructions_; }
  const auto& instructions() const { return Instructions_; }
  void append(std::unique_ptr<TargetInstruction> TI) {
    Instructions_.push_back(std::move(TI));
  }
};

/// 目标栈帧信息
struct TargetFrameInfo {
  uint64_t StackSize = 0;
  uint64_t LocalVarOffset = 0;
  unsigned NumSpillSlots = 0;
  unsigned MaxCallFrameSize = 0;
};

/// 目标函数（后端 IR 的函数级别表示）
class TargetFunction {
  std::string Name_;
  ir::IRFunctionType* Signature_;  // 非拥有指针，生命周期由 IRTypeContext 管理
  std::vector<TargetBasicBlock> Blocks_;
  TargetFrameInfo FrameInfo_;
public:
  TargetFunction() : Signature_(nullptr) {}
  TargetFunction(ir::StringRef Name, ir::IRFunctionType* Sig)
    : Name_(Name.str()), Signature_(Sig) {}

  ir::StringRef getName() const { return Name_; }
  ir::IRFunctionType* getSignature() const { return Signature_; }
  std::vector<TargetBasicBlock>& getBlocks() { return Blocks_; }
  const std::vector<TargetBasicBlock>& getBlocks() const { return Blocks_; }
  TargetFrameInfo& getFrameInfo() { return FrameInfo_; }
  const TargetFrameInfo& getFrameInfo() const { return FrameInfo_; }
};

/// 目标平台寄存器信息
class TargetRegisterInfo {
  unsigned NumRegisters_ = 0;
  ir::SmallVector<unsigned, 16> CalleeSavedRegs_;
  ir::SmallVector<unsigned, 16> CallerSavedRegs_;
  ir::DenseMap<unsigned, unsigned> RegClassMap_;
public:
  unsigned getNumRegisters() const { return NumRegisters_; }
  void setNumRegisters(unsigned N) { NumRegisters_ = N; }

  ir::ArrayRef<unsigned> getCalleeSavedRegs() const { return CalleeSavedRegs_; }
  ir::ArrayRef<unsigned> getCallerSavedRegs() const { return CallerSavedRegs_; }

  void addCalleeSavedReg(unsigned Reg) { CalleeSavedRegs_.push_back(Reg); }
  void addCallerSavedReg(unsigned Reg) { CallerSavedRegs_.push_back(Reg); }

  bool isCalleeSaved(unsigned Reg) const;
  bool isCallerSaved(unsigned Reg) const;

  void setRegClass(unsigned Reg, unsigned ClassID) { RegClassMap_[Reg] = ClassID; }
  unsigned getRegClass(unsigned Reg) const;
};

// === 寄存器分配器 ===

/// 寄存器分配器抽象基类
class RegisterAllocator {
public:
  virtual ~RegisterAllocator() = default;
  virtual ir::StringRef getName() const = 0;
  virtual RegAllocStrategy getStrategy() const = 0;

  /// 为 TargetFunction 中的虚拟寄存器分配物理寄存器
  /// \returns true if allocation succeeded, false on spill failure
  virtual bool allocate(TargetFunction& F,
                        const TargetRegisterInfo& TRI) = 0;
};

/// 寄存器分配器工厂
class RegAllocFactory {
public:
  static std::unique_ptr<RegisterAllocator> create(RegAllocStrategy Strategy);
};

} // namespace blocktype::backend
