include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

add_library(engine STATIC)

target_sources(engine
PRIVATE
    core/Engine.cpp
    platform/window/Window.cpp
)

target_include_directories(engine
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Vulkan REQUIRED)

target_link_libraries(engine
PUBLIC
    Vulkan::Vulkan
PRIVATE
    glfw
)
