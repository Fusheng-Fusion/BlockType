add_executable(blocktype-integration-tests
  AI/ProviderIntegrationTest.cpp
  backend/BackendIntegrationTest.cpp
  pipeline/D2PipelineTest.cpp
  pipeline/D3FallbackTest.cpp
  pipeline/D4RegistrationTest.cpp
  e2e/E2ECompileTest.cpp
)

target_link_libraries(blocktype-integration-tests
  blocktype-basic
  blocktype-ai
  blocktypeBackend
  blocktypeFrontend
  blocktype-ir
  ${LLVM_LIBS}
  GTest::gtest_main
)

target_include_directories(blocktype-integration-tests
  PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

gtest_discover_tests(blocktype-integration-tests)
