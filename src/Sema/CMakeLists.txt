add_library(blocktype-sema
  Lookup.cpp
  Scope.cpp
  Sema.cpp
  SymbolTable.cpp
  TypeDeduction.cpp
)

target_link_libraries(blocktype-sema
  PUBLIC
    blocktype-ast
)