# --------------------------------ONNX Runtime--------------------------------

set(ONNXRUNTIME_VERSION "1.30.0")

if(WIN32)

    set(ONNXRUNTIME_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}"
    )

    set(ONNXRUNTIME_LIBRARY
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.lib"
    )

    set(ONNXRUNTIME_RUNTIME
        "${ONNXRUNTIME_ROOT}/lib/onnxruntime.dll"
    )

elseif(UNIX AND NOT APPLE)

    set(ONNXRUNTIME_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}"
    )

    find_library(
        ONNXRUNTIME_LIBRARY
        NAMES onnxruntime
        PATHS "${ONNXRUNTIME_ROOT}/lib"
        NO_DEFAULT_PATH
        REQUIRED
    )

else()

    message(FATAL_ERROR "Unsupported platform for ONNX Runtime")

endif()


set(ONNXRUNTIME_INCLUDE_DIR
    "${ONNXRUNTIME_ROOT}/include"
)

if(NOT EXISTS "${ONNXRUNTIME_INCLUDE_DIR}")
    message(FATAL_ERROR
        "ONNX Runtime include directory not found: ${ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()


# 创建统一的 ONNX Runtime imported target
if(WIN32)

    add_library(
        ONNXRuntime::ONNXRuntime
        SHARED IMPORTED GLOBAL
    )

    set_target_properties(
        ONNXRuntime::ONNXRuntime
        PROPERTIES
            IMPORTED_IMPLIB "${ONNXRUNTIME_LIBRARY}"
            IMPORTED_LOCATION "${ONNXRUNTIME_RUNTIME}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )

else()

    add_library(
        ONNXRuntime::ONNXRuntime
        SHARED IMPORTED GLOBAL
    )

    set_target_properties(
        ONNXRuntime::ONNXRuntime
        PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )

endif()


message(STATUS "ONNX Runtime root: ${ONNXRUNTIME_ROOT}")
message(STATUS "ONNX Runtime library: ${ONNXRUNTIME_LIBRARY}")

# ------------------------------ONNX Runtime End------------------------------