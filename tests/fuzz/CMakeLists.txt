add_library(blocktype-fuzz
  FrontendFuzzer.cpp
)
target_include_directories(blocktype-fuzz
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(blocktype-fuzz
  PUBLIC blocktype-ir
)
