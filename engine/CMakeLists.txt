add_library(engine STATIC)

target_sources(engine
PRIVATE
    core/Engine.cpp
)

target_include_directories(engine
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(Vulkan REQUIRED)

target_link_libraries(engine
PUBLIC
    Vulkan::Vulkan
)
