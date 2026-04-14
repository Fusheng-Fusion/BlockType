add_library(blocktype-codegen INTERFACE)
target_link_libraries(blocktype-codegen INTERFACE blocktype-sema ${LLVM_LIBS})