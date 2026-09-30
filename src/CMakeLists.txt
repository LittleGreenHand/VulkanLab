file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER ALL_SOURCES EXCLUDE REGEX ".*/AI/.*")

add_executable(${PROJECT_NAME}
    ${ALL_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ALL_SOURCES})

vulkanlab_deploy_dependencies(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Vulkan::Vulkan
    ${WINLIBS}
    imgui
    ktx
    glfw
    ${OpenCV_LIBS}
    ${AI_PROJECT_NAME}
    VulkanLabDependencies
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 oleaut32 strmiids)
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ENGINE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    SHADERS_DIR="${CMAKE_SOURCE_DIR}/shaders"
    SHADERS_SPV_DIR="${SHADERS_OUTPUT}"
)

add_dependencies(${PROJECT_NAME} Shaders)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    ENABLE_LOG=$<BOOL:${ENABLE_LOG}>
    ENABLE_DEBUG_LOG=$<BOOL:${ENABLE_DEBUG_LOG}>
    GLM_ENABLE_EXPERIMENTAL
)


# --------------------------------OpenCV----------------------------------
if(WIN32)
    set(OpenCV_BIN_DIR "${OpenCV_LIB_PATH}/../bin")
    file(GLOB OpenCV_DLLS "${OpenCV_BIN_DIR}/opencv*.dll")

    set(OpenCV_DLLS_RELEASE ${OpenCV_DLLS})
    list(FILTER OpenCV_DLLS_RELEASE EXCLUDE REGEX "d\\.dll$")
    
    # 自动将OpenCV的dll拷贝到可执行文件目录下
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<STREQUAL:${VULKANLAB_DEPENDENCY_CONFIG},Release>,${OpenCV_DLLS_RELEASE},${OpenCV_DLLS}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND_EXPAND_LISTS
    )
endif()
# ------------------------------OpenCV End--------------------------------

# --------------------------------ONNX Runtime--------------------------------

if(WIN32)

    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:ONNXRuntime::ONNXRuntime>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )

endif()

# ------------------------------ONNX Runtime End------------------------------
