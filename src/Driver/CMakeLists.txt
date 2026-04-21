# Driver 库 - 编译器驱动和工具链集成

add_library(blocktype-driver
  ModuleLinker.cpp
)

target_include_directories(blocktype-driver
  PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(blocktype-driver
  PUBLIC
    blocktype-module
    LLVM
)
