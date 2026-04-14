# LLVM 配置

# 查找 LLVM 包
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# LLVM include 路径
include_directories(${LLVM_INCLUDE_DIRS})

# LLVM 定义
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# LLVM 组件
llvm_map_components_to_libnames(LLVM_LIBS
  core
  support
  orcjit
  native
  asmparser
  bitwriter
  irreader
  transformutils
  x86asmparser
  x86codegen
  x86desc
  x86info
  aarch64asmparser
  aarch64codegen
  aarch64desc
  aarch64info
)

message(STATUS "LLVM libraries: ${LLVM_LIBS}")
