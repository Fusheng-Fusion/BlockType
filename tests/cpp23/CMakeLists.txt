# tests/cpp23/CMakeLists.txt
# C++23 feature end-to-end compile tests for BlockType compiler

# Helper function: register a compile test with the BlockType compiler
function(add_blocktype_compile_test name source_file)
  add_test(
    NAME "cpp23.${name}"
    COMMAND blocktype --emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/${source_file} -o /dev/null
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties("cpp23.${name}" PROPERTIES
    TIMEOUT 30
    LABELS "cpp23;e2e"
    REQUIRED_FILES "${CMAKE_BINARY_DIR}/tools/blocktype"
  )
endfunction()

# C++23 feature tests
add_blocktype_compile_test(deducing_this test_deducing_this.cpp)
add_blocktype_compile_test(decay_copy test_decay_copy.cpp)
add_blocktype_compile_test(static_operator test_static_operator.cpp)
add_blocktype_compile_test(assume_attribute test_assume_attribute.cpp)
add_blocktype_compile_test(if_consteval test_if_consteval.cpp)
add_blocktype_compile_test(multidim_subscript test_multidim_subscript.cpp)
add_blocktype_compile_test(preprocessor test_preprocessor.cpp)
add_blocktype_compile_test(lambda_enhancements test_lambda_enhancements.cpp)
add_blocktype_compile_test(literal_suffixes test_literal_suffixes.cpp)
add_blocktype_compile_test(escape_sequences test_escape_sequences.cpp)
