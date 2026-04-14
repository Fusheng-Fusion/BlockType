#pragma once

#include <string>

namespace blocktype {

struct AIConfig {
  std::string OpenAIKey;
  std::string ClaudeKey;
  std::string QwenKey;
  std::string OllamaEndpoint = "http://localhost:11434";
  
  bool EnableCache = true;
  unsigned MaxCacheSize = 1000;
  unsigned TimeoutMs = 30000;
  double MaxCostPerDay = 10.0;
  
  static AIConfig fromEnvironment();
  static AIConfig fromFile(const std::string& Path);
};

} // namespace blocktype