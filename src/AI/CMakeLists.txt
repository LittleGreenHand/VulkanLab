file(GLOB_RECURSE AI_SOURCES_FILE CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(${AI_PROJECT_NAME} STATIC
    ${AI_SOURCES_FILE}
)

source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${AI_SOURCES_FILE}
)

target_include_directories(${AI_PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"

    PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_compile_definitions(${AI_PROJECT_NAME}
    PRIVATE
        ENABLE_LOG=$<BOOL:${ENABLE_LOG}>
        ENABLE_DEBUG_LOG=$<BOOL:${ENABLE_DEBUG_LOG}>
)

target_link_libraries(${AI_PROJECT_NAME}
    PUBLIC
        ${OpenCV_LIBS}
        ONNXRuntime::ONNXRuntime
)
