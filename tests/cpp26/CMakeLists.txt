# tests/cpp26/CMakeLists.txt
# C++26 feature end-to-end compile tests for BlockType compiler

# Helper function: register a compile test with the BlockType compiler
function(add_blocktype_compile_test name source_file)
  add_test(
    NAME "cpp26.${name}"
    COMMAND blocktype --emit-llvm ${CMAKE_CURRENT_SOURCE_DIR}/${source_file} -o /dev/null
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  set_tests_properties("cpp26.${name}" PROPERTIES
    TIMEOUT 30
    LABELS "cpp26;e2e"
    REQUIRED_FILES "${CMAKE_BINARY_DIR}/tools/blocktype"
  )
endfunction()

# C++26 feature tests
add_blocktype_compile_test(pack_indexing test_pack_indexing.cpp)
add_blocktype_compile_test(embed test_embed.cpp)
add_blocktype_compile_test(reflexpr test_reflexpr.cpp)
add_blocktype_compile_test(contracts test_contracts.cpp)
add_blocktype_compile_test(contracts_inheritance contracts_inheritance.cpp)
add_blocktype_compile_test(contracts_virtual contracts_virtual.cpp)
add_blocktype_compile_test(contracts_mode contracts_mode.cpp)
add_blocktype_compile_test(integration test_integration.cpp)
# Supplementary tests for Stage 7.5 features
add_blocktype_compile_test(delimited_escape test_delimited_escape.cpp)
add_blocktype_compile_test(named_escape test_named_escape.cpp)
add_blocktype_compile_test(for_init_using test_for_init_using.cpp)
add_blocktype_compile_test(extended_identifiers test_extended_identifiers.cpp)
add_blocktype_compile_test(indeterminate test_indeterminate.cpp)
add_blocktype_compile_test(trivially_relocatable test_trivially_relocatable.cpp)
add_blocktype_compile_test(concept_template_param test_concept_template_param.cpp)
add_blocktype_compile_test(variadic_friend test_variadic_friend.cpp)
add_blocktype_compile_test(constexpr_exception test_constexpr_exception.cpp)
