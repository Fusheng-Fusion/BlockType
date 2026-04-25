# libblocktype-ir-cache — 编译缓存子库
add_library(blocktype-ir-cache
  IRCache.cpp
)

target_include_directories(blocktype-ir-cache
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir-cache
  PUBLIC
    blocktype-basic
    blocktype-ir
)

install(TARGETS blocktype-ir-cache
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
