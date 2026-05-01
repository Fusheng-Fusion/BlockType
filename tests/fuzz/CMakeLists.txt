add_library(blocktype-fuzz
  FrontendFuzzer.cpp
)

# IRFuzzer standalone test executable
add_executable(ir-fuzzer-test
  IRFuzzer.cpp
)
target_include_directories(ir-fuzzer-test
  PRIVATE ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(ir-fuzzer-test
  PRIVATE blocktype-ir
)
target_include_directories(blocktype-fuzz
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(blocktype-fuzz
  PUBLIC blocktype-ir
)
