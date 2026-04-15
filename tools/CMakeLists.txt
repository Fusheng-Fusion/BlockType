add_executable(blocktype
  driver.cpp
)

target_link_libraries(blocktype PRIVATE
  blocktype-basic
  blocktype-lex
  blocktype-parse
  blocktype-ast
  blocktype-codegen
  blocktype-ai
  ${LLVM_LIBS}
)

blocktype_add_compile_options(blocktype)