add_executable(marble_pack_v1
    marble_pack_v1.cpp
)

target_include_directories(marble_pack_v1
PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(marble_pack_v1
PRIVATE
    marble_compile_options
)

add_executable(marble_mesh_v1
    marble_mesh_v1.cpp
)

target_include_directories(marble_mesh_v1
PRIVATE
    ${CMAKE_SOURCE_DIR}/engine
)

target_link_libraries(marble_mesh_v1
PRIVATE
    marble_compile_options
)
