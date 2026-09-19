file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_executable(${PROJECT_NAME}
    ${ALL_SOURCES}
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${ALL_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Vulkan::Vulkan
    ${WINLIBS}
    imgui
    ktx
    glfw
    ${OpenCV_LIBS}
)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

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

option(ENABLE_LOG "是否启用LOG并在控制台输出" ON)
option(ENABLE_DEBUG_LOG "在启用LOG的前提下，是否启用DEBUG LOG" ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ENABLE_LOG=$<BOOL:${ENABLE_LOG}>
    ENABLE_DEBUG_LOG=$<BOOL:${ENABLE_DEBUG_LOG}>
    GLM_ENABLE_EXPERIMENTAL
)

# --------------------------------PhysX--------------------------------
set(PHYSX_ROOT "${CMAKE_SOURCE_DIR}/thirdParty/PhysX/physx")

target_include_directories(${PROJECT_NAME} PRIVATE "${PHYSX_ROOT}/include")

if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        PhysX PhysXCommon PhysXCooking PhysXFoundation PhysXExtensions PhysXPvdSDK
    )
else()
    set(PHYSX_BIN_DIR "${PHYSX_ROOT}/bin/win.x86_64.vc143.md" CACHE PATH "PhysX二进制文件目录")
    set(PHYSX_CONFIG_DIR "$<IF:$<CONFIG:Debug>,debug,release>")
    target_link_directories(${PROJECT_NAME} PRIVATE "${PHYSX_BIN_DIR}/${PHYSX_CONFIG_DIR}")
    target_link_libraries(${PROJECT_NAME} PRIVATE
        PhysX_64 PhysXCommon_64 PhysXCooking_64 PhysXFoundation_64
        PhysXExtensions_static_64 PhysXPvdSDK_static_64
    )
    # 自动将PhysX的dll拷贝到可执行文件目录下
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PHYSX_BIN_DIR}/${PHYSX_CONFIG_DIR}/PhysX_64.dll"
            "${PHYSX_BIN_DIR}/${PHYSX_CONFIG_DIR}/PhysXCommon_64.dll"
            "${PHYSX_BIN_DIR}/${PHYSX_CONFIG_DIR}/PhysXCooking_64.dll"
            "${PHYSX_BIN_DIR}/${PHYSX_CONFIG_DIR}/PhysXFoundation_64.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()
#--------------------------------PhysX End--------------------------------

# --------------------------------OpenCV----------------------------------
if(WIN32)
    set(OpenCV_BIN_DIR "${OpenCV_LIB_PATH}/../bin")
    file(GLOB OpenCV_DLLS "${OpenCV_BIN_DIR}/opencv*.dll")

    set(OpenCV_DLLS_RELEASE ${OpenCV_DLLS})
    list(FILTER OpenCV_DLLS_RELEASE EXCLUDE REGEX "d\\.dll$")
    
    # 自动将OpenCV的dll拷贝到可执行文件目录下
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<IF:$<CONFIG:Release>,${OpenCV_DLLS_RELEASE},${OpenCV_DLLS}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND_EXPAND_LISTS
    )
endif()
# ------------------------------OpenCV End--------------------------------