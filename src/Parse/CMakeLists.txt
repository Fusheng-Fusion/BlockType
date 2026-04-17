add_library(blocktype-parse
  Declarator.cpp
  OperatorPrecedence.cpp
  ParseClass.cpp
  ParseDecl.cpp
  ParseDeclSpec.cpp
  ParseExpr.cpp
  ParseExprCXX.cpp
  ParseStmt.cpp
  ParseStmtCXX.cpp
  ParseTemplate.cpp
  Parser.cpp
  ParseType.cpp
)

target_include_directories(blocktype-parse
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
)

target_link_libraries(blocktype-parse
  PUBLIC
    blocktype-ast
    blocktype-lex
    blocktype-basic
    blocktype-sema
)
