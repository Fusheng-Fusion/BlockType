# libblocktype-ir-telemetry — 遥测子库
add_library(blocktype-ir-telemetry
  IRTelemetry.cpp
)

target_include_directories(blocktype-ir-telemetry
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(blocktype-ir-telemetry
  PUBLIC
    blocktype-basic
)

install(TARGETS blocktype-ir-telemetry
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
