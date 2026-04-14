# Benchmark tests for BlockType

add_executable(lexer-benchmark LexerBenchmark.cpp)

target_link_libraries(lexer-benchmark
  PRIVATE
  blocktype-lex
  blocktype-basic
  LLVMSupport
)

set_target_properties(lexer-benchmark PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
