# Backend diff tests (Phase D-F5)

add_executable(BackendDiffTest
  BackendDiffTest.cpp
)

target_link_libraries(BackendDiffTest
  PRIVATE
    blocktypeBackend
    blocktypeFrontend
    blocktype-basic
    blocktype-ir
    ${LLVM_LIBS}
    GTest::gtest
    GTest::gtest_main
)

target_include_directories(BackendDiffTest
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(BackendDiffTest)
