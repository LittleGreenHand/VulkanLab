# Application configurations and the installed SDK configuration are independent.
set(VULKANLAB_DEPENDENCY_CONFIG Release CACHE STRING "Link Debug or Release dependency SDKs")
set_property(CACHE VULKANLAB_DEPENDENCY_CONFIG PROPERTY STRINGS Debug Release)
if(NOT VULKANLAB_DEPENDENCY_CONFIG MATCHES "^(Debug|Release)$")
    message(FATAL_ERROR "VULKANLAB_DEPENDENCY_CONFIG must be Debug or Release")
endif()
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Application configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Application build configuration" FORCE)
endif()
if(DEFINED CACHE{CMAKE_BUILD_TYPE})
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

# Initialize all imported targets, including OpenCV, with the selected SDK ABI.
foreach(_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_MAP_IMPORTED_CONFIG_${_config} "${VULKANLAB_DEPENDENCY_CONFIG};")
endforeach()
if(MSVC)
    if(VULKANLAB_DEPENDENCY_CONFIG STREQUAL "Release")
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0 NDEBUG)
        add_compile_options(/U_DEBUG /UDEBUG)
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDebugDLL)
        add_compile_definitions(_ITERATOR_DEBUG_LEVEL=2 _DEBUG)
        add_compile_options(/UNDEBUG)
    endif()
endif()

if(WIN32)
    set(_dependency_platform windows-x86_64)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_dependency_platform linux-x86_64)
else()
    message(FATAL_ERROR "Dependency SDKs support Windows and Linux x86_64")
endif()

string(TOLOWER "${VULKANLAB_DEPENDENCY_CONFIG}" _dependency_config)
set(_sdk_suffix "install/${_dependency_platform}/${_dependency_config}")
set(TBB_DIR "${PROJECT_SOURCE_DIR}/thirdParty/oneTBB/${_sdk_suffix}/lib/cmake/TBB")
set(pxr_DIR "${PROJECT_SOURCE_DIR}/thirdParty/OpenUSD/${_sdk_suffix}")
set(PhysX_DIR "${PROJECT_SOURCE_DIR}/thirdParty/PhysX/${_sdk_suffix}/lib/cmake/PhysX")
foreach(_package IN ITEMS "${TBB_DIR}/TBBConfig.cmake" "${pxr_DIR}/pxrConfig.cmake" "${PhysX_DIR}/PhysXConfig.cmake")
    if(NOT EXISTS "${_package}")
        message(FATAL_ERROR "Missing SDK package: ${_package}\nRun: python build_dependencies.py --config ${VULKANLAB_DEPENDENCY_CONFIG}")
    endif()
endforeach()

find_package(TBB CONFIG REQUIRED COMPONENTS tbb PATHS "${TBB_DIR}" NO_DEFAULT_PATH)
find_package(pxr CONFIG REQUIRED PATHS "${pxr_DIR}" NO_DEFAULT_PATH)
find_package(PhysX CONFIG REQUIRED PATHS "${PhysX_DIR}" NO_DEFAULT_PATH)
add_library(VulkanLabDependencies INTERFACE)
target_link_libraries(VulkanLabDependencies INTERFACE
  TBB::tbb
  PhysX::physx_lib
)
message(STATUS "VulkanLab SDKs: ${VULKANLAB_SDK_ROOT}")
add_library(USDDependencies INTERFACE)
set(OPENUSD_INCLUDE_DIR "${pxr_DIR}/include")
target_include_directories(USDDependencies INTERFACE "${OPENUSD_INCLUDE_DIR}")
target_link_libraries(USDDependencies INTERFACE
    TBB::tbb
    usd
    usdGeom
    usdShade
    usdLux
    usdPhysics
    usdUtils
)

function(vulkanlab_deploy_dependencies target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
            COMMAND_EXPAND_LISTS VERBATIM)
        # USD locates its schema/plugin metadata relative to the USD libraries.
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${pxr_DIR}/lib/usd" "$<TARGET_FILE_DIR:${target}>/usd"
            VERBATIM)
        if(EXISTS "${pxr_DIR}/plugin/usd")
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${pxr_DIR}/plugin/usd" "$<TARGET_FILE_DIR:${target}>/../plugin/usd"
                VERBATIM)
        endif()
    endif()
endfunction()
