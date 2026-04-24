add_library(blocktype-ir
  IRConstant.cpp
  IRContext.cpp
  IRFormat.cpp
  IRInstruction.cpp
  IRType.cpp
  IRTypeContext.cpp
  IRValue.cpp
  TargetLayout.cpp
)

target_include_directories(blocktype-ir
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir
  PRIVATE
    blocktype-basic
)

install(TARGETS blocktype-ir
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/blocktype/IR
  DESTINATION include/blocktype
  FILES_MATCHING PATTERN "*.h"
)
