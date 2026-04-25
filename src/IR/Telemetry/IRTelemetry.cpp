#include "blocktype/IR/IRTelemetry.h"

#include <chrono>
#include <fstream>
#include <string>

#ifdef __APPLE__
#include <mach/mach.h>
#endif

namespace blocktype {
namespace telemetry {

static const char* phaseName(CompilationPhase P) {
  switch (P) {
  case CompilationPhase::Frontend:       return "Frontend";
  case CompilationPhase::IRGeneration:   return "IRGeneration";
  case CompilationPhase::IRValidation:   return "IRValidation";
  case CompilationPhase::IROptimization: return "IROptimization";
  case CompilationPhase::BackendCodegen: return "BackendCodegen";
  case CompilationPhase::BackendOptimize:return "BackendOptimize";
  case CompilationPhase::CodeEmission:   return "CodeEmission";
  case CompilationPhase::Linking:        return "Linking";
  }
  return "Unknown";
}

static std::string jsonEscape(const std::string& S) {
  std::string Out;
  Out.reserve(S.size());
  for (char C : S) {
    switch (C) {
    case '"':  Out += "\\\""; break;
    case '\\': Out += "\\\\"; break;
    case '\n': Out += "\\n"; break;
    case '\r': Out += "\\r"; break;
    case '\t': Out += "\\t"; break;
    default:   Out += C; break;
    }
  }
  return Out;
}

// --- TelemetryCollector static utility methods ---

uint64_t TelemetryCollector::getCurrentTimeNs() {
  auto Now = std::chrono::steady_clock::now();
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      Now.time_since_epoch()).count());
}

size_t TelemetryCollector::getCurrentMemoryUsage() {
#ifdef __APPLE__
  task_basic_info_data_t Info;
  mach_msg_type_number_t Count = TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&Info), &Count) == KERN_SUCCESS) {
    return Info.resident_size;
  }
  return 0;
#elif defined(__linux__)
  // Read VmRSS from /proc/self/status
  std::ifstream ifs("/proc/self/status");
  std::string Line;
  while (std::getline(ifs, Line)) {
    if (Line.compare(0, 6, "VmRSS:") == 0) {
      // VmRSS:    12345 kB
      size_t Val = 0;
      sscanf(Line.c_str(), "VmRSS: %zu kB", &Val);
      return Val * 1024;
    }
  }
  return 0;
#else
  return 0;
#endif
}

// --- PhaseGuard ---

TelemetryCollector::PhaseGuard::PhaseGuard(
    TelemetryCollector& C, CompilationPhase P, StringRef D)
  : Collector(C), Phase(P), Detail(D.str()),
    StartNs(TelemetryCollector::getCurrentTimeNs()),
    MemoryBefore(TelemetryCollector::getCurrentMemoryUsage()) {}

TelemetryCollector::PhaseGuard::~PhaseGuard() {
  if (Collector.Enabled && !MovedFrom_) {
    CompilationEvent E;
    E.Phase = Phase;
    E.Detail = Detail;
    E.StartNs = StartNs;
    E.EndNs = TelemetryCollector::getCurrentTimeNs();
    E.MemoryBefore = MemoryBefore;
    E.MemoryAfter = TelemetryCollector::getCurrentMemoryUsage();
    E.Success = !Failed;
    Collector.Events.push_back(std::move(E));
  }
}

// --- TelemetryCollector ---

bool TelemetryCollector::writeChromeTrace(StringRef Path) const {
  std::ofstream OF(Path.str());
  if (!OF.is_open()) return false;

  OF << "{\n  \"traceEvents\": [\n";
  for (size_t i = 0; i < Events.size(); ++i) {
    const auto& E = Events[i];
    uint64_t TsBegin = E.StartNs / 1000;
    uint64_t TsEnd   = E.EndNs / 1000;

    // Begin event
    OF << "    {\"ph\":\"B\",\"pid\":0,\"tid\":0,\"ts\":" << TsBegin
       << ",\"name\":\"" << phaseName(E.Phase) << "\""
       << ",\"cat\":\"compiler\""
       << ",\"args\":{\"detail\":\"" << jsonEscape(E.Detail) << "\""
       << ",\"memoryBefore\":" << E.MemoryBefore
       << ",\"memoryAfter\":" << E.MemoryAfter << "}}";

    OF << ",\n";
    // End event
    OF << "    {\"ph\":\"E\",\"pid\":0,\"tid\":0,\"ts\":" << TsEnd << "}";

    if (i + 1 < Events.size()) OF << ",";
    OF << "\n";
  }
  OF << "  ],\n";
  OF << "  \"displayTimeUnit\": \"us\",\n";
  OF << "  \"metadata\": {\"compiler\": \"BlockType\", \"version\": \"1.0.0\"}\n";
  OF << "}\n";

  return true;
}

bool TelemetryCollector::writeJSONReport(StringRef Path) const {
  std::ofstream OF(Path.str());
  if (!OF.is_open()) return false;

  uint64_t TotalNs = 0;
  size_t MaxMemory = 0;

  OF << "{\n  \"events\": [\n";
  for (size_t i = 0; i < Events.size(); ++i) {
    const auto& E = Events[i];
    uint64_t DurNs = (E.EndNs > E.StartNs) ? (E.EndNs - E.StartNs) : 0;
    TotalNs += DurNs;
    if (E.MemoryAfter > MaxMemory) MaxMemory = E.MemoryAfter;

    OF << "    {\"phase\":\"" << phaseName(E.Phase) << "\""
       << ",\"detail\":\"" << jsonEscape(E.Detail) << "\""
       << ",\"durationNs\":" << DurNs
       << ",\"memoryBefore\":" << E.MemoryBefore
       << ",\"memoryAfter\":" << E.MemoryAfter
       << ",\"success\":" << (E.Success ? "true" : "false") << "}";
    if (i + 1 < Events.size()) OF << ",";
    OF << "\n";
  }
  OF << "  ],\n";
  OF << "  \"summary\": {\n";
  OF << "    \"totalDurationNs\": " << TotalNs << ",\n";
  OF << "    \"peakMemoryUsage\": " << MaxMemory << ",\n";
  OF << "    \"eventCount\": " << Events.size() << "\n";
  OF << "  }\n";
  OF << "}\n";

  return true;
}

} // namespace telemetry
} // namespace blocktype
