add_executable(blocktype-module-test
  ModuleLinkerTest.cpp
  ModuleManagerTest.cpp
  ModuleVisibilityTest.cpp
)

target_link_libraries(blocktype-module-test
  PRIVATE
    blocktype-driver
    blocktype-module
    blocktype-sema
    gtest
    gtest_main
)

add_test(NAME ModuleManagerTest COMMAND blocktype-module-test)
