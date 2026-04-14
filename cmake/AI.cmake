# AI 功能依赖配置

# libcurl (HTTP 客户端)
find_package(CURL QUIET)
if(CURL_FOUND)
  message(STATUS "Found CURL: ${CURL_LIBRARIES}")
  message(STATUS "CURL include: ${CURL_INCLUDE_DIRS}")
else()
  message(STATUS "CURL not found, AI features will be limited")
endif()

# nlohmann/json (JSON 解析)
include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(json)

message(STATUS "nlohmann/json configured")
