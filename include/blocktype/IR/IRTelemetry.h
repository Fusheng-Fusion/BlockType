#ifndef BLOCKTYPE_IR_IRTELEMETRY_H
#define BLOCKTYPE_IR_IRTELEMETRY_H

#include "blocktype/IR/ADT.h"
#include <cstdint>
#include <string>

namespace blocktype {
namespace telemetry {

using blocktype::ir::SmallVector;
using blocktype::ir::StringRef;

enum class CompilationPhase : uint8_t {
  Frontend       = 0,
  IRGeneration   = 1,
  IRValidation   = 2,
  IROptimization = 3,
  BackendCodegen = 4,
  BackendOptimize = 5,
  CodeEmission   = 6,
  Linking        = 7,
};

struct CompilationEvent {
  CompilationPhase Phase;
  std::string Detail;
  uint64_t StartNs;
  uint64_t EndNs;
  size_t MemoryBefore;
  size_t MemoryAfter;
  bool Success;
};

class TelemetryCollector {
  bool Enabled = false;
  SmallVector<CompilationEvent, 64> Events;
  uint64_t CompilationStartNs = 0;
public:
  static uint64_t getCurrentTimeNs();
  static size_t getCurrentMemoryUsage();

  void enable() {
    Enabled = true;
    CompilationStartNs = getCurrentTimeNs();
  }
  bool isEnabled() const { return Enabled; }

  class PhaseGuard {
    TelemetryCollector& Collector;
    CompilationPhase Phase;
    std::string Detail;
    uint64_t StartNs;
    size_t MemoryBefore;
    bool Failed = false;
    bool MovedFrom_ = false;
  public:
    PhaseGuard(TelemetryCollector& C, CompilationPhase P, StringRef D);
    ~PhaseGuard();
    PhaseGuard(const PhaseGuard&) = delete;
    PhaseGuard& operator=(const PhaseGuard&) = delete;
    PhaseGuard(PhaseGuard&& Other) noexcept
      : Collector(Other.Collector), Phase(Other.Phase),
        Detail(std::move(Other.Detail)), StartNs(Other.StartNs),
        MemoryBefore(Other.MemoryBefore), Failed(Other.Failed) {
      Other.MovedFrom_ = true;
    }
    PhaseGuard& operator=(PhaseGuard&&) = delete;
    void markFailed() { Failed = true; }
  };

  PhaseGuard scopePhase(CompilationPhase P, StringRef Detail = "") {
    return PhaseGuard(*this, P, Detail);
  }

  const SmallVector<CompilationEvent, 64>& getEvents() const { return Events; }
  void clear() { Events.clear(); }
  bool writeChromeTrace(StringRef Path) const;
  bool writeJSONReport(StringRef Path) const;
};

} // namespace telemetry
} // namespace blocktype

#endif
