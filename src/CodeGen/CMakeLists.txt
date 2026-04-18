add_library(blocktype-codegen
  CGCXX.cpp
  CodeGenConstant.cpp
  CodeGenFunction.cpp
  CodeGenModule.cpp
  CodeGenStmt.cpp
  CodeGenTypes.cpp
  CodeGenExpr.cpp
  TargetInfo.cpp
)

target_link_libraries(blocktype-codegen
  PUBLIC
    blocktype-sema
    ${LLVM_LIBS}
)
