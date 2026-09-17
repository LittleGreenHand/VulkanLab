set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_impl_glfw.cpp
    imgui_impl_vulkan.cpp
)
add_library(imgui STATIC
    ${IMGUI_SOURCES}
)
target_link_libraries(imgui
    PRIVATE
    Vulkan::Vulkan
    glfw
)
target_include_directories(imgui
    PUBLIC
    ${IMGUI_DIR}
)

