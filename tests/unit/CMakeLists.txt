add_executable(blocktype-tests
  Basic/SourceLocationTest.cpp
)

target_link_libraries(blocktype-tests
  blocktype-basic
  GTest::gtest_main
)

gtest_discover_tests(blocktype-tests)