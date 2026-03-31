find_program(MARBLE_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
if(NOT MARBLE_GLSLC)
    message(FATAL_ERROR "glslc not found (install LunarG Vulkan SDK and set VULKAN_SDK, or add glslc to PATH)")
endif()

set(MARBLE_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/marbles/shaders)
set(MARBLE_SHADER_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/marbles_shaders)
file(MAKE_DIRECTORY ${MARBLE_SHADER_SPV_DIR})

set(MARBLE_SHADER_SPVS "")
foreach(NAME mesh.vert mesh.frag)
    set(SPV ${MARBLE_SHADER_SPV_DIR}/${NAME}.spv)
    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${MARBLE_GLSLC} --target-env=vulkan1.2 -o ${SPV} ${MARBLE_SHADER_SRC_DIR}/${NAME}
        DEPENDS ${MARBLE_SHADER_SRC_DIR}/${NAME}
        COMMENT "Compile ${NAME} -> SPIR-V"
        VERBATIM
    )
    list(APPEND MARBLE_SHADER_SPVS ${SPV})
endforeach()

add_custom_target(marbles_shaders DEPENDS ${MARBLE_SHADER_SPVS})

add_executable(marbles)

target_sources(marbles
PRIVATE
    marbles/main.cpp
    marbles/MarblesGame.cpp
)

add_dependencies(marbles marbles_shaders)

target_link_libraries(marbles
PRIVATE
    engine
)

add_custom_command(
    TARGET marbles
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${MARBLE_SHADER_SPV_DIR}
            $<TARGET_FILE_DIR:marbles>/shaders
    COMMENT "Stage SPIR-V shaders next to marbles"
    VERBATIM
)

add_custom_command(
    TARGET marbles
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:marbles>/assets
    COMMENT "Stage runtime assets next to marbles"
    VERBATIM
)
