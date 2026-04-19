# AST unit tests

add_executable(ast-tests
  ASTTest.cpp
  Stage71Test.cpp
  Stage72Test.cpp
  Stage73Test.cpp
)

target_link_libraries(ast-tests
  PRIVATE
    blocktype-ast
    blocktype-sema
    blocktype-basic
    GTest::gtest_main
)

gtest_discover_tests(ast-tests)
