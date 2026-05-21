set(_marble_glslc_hints "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")

if(DEFINED VCPKG_TARGET_TRIPLET AND DEFINED CMAKE_TOOLCHAIN_FILE)

    string(FIND "${CMAKE_TOOLCHAIN_FILE}" "vcpkg.cmake" _marble_vcpkg_tf)

    if(NOT _marble_vcpkg_tf EQUAL -1)

        get_filename_component(_marble_v1 "${CMAKE_TOOLCHAIN_FILE}" DIRECTORY)

        get_filename_component(_marble_v2 "${_marble_v1}" DIRECTORY)

        get_filename_component(_marble_vroot "${_marble_v2}" DIRECTORY)

        list(APPEND _marble_glslc_hints "${_marble_vroot}/installed/${VCPKG_TARGET_TRIPLET}/tools/shaderc")

    endif()

endif()

find_program(MARBLE_GLSLC NAMES glslc HINTS ${_marble_glslc_hints})

if(NOT MARBLE_GLSLC)

    message(FATAL_ERROR "glslc not found (install LunarG Vulkan SDK and set VULKAN_SDK, or add glslc to PATH)")

endif()

set(MARBLE_GLSLC_TARGET_ENV "vulkan1.2" CACHE STRING "glslc --target-env for Marble sample shaders")

set(MARBLE_GLSLC_EXTRA_FLAGS "" CACHE STRING "Extra glslc args for sample shaders (CMake list, e.g. -g or -O)")

set(MARBLE_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/marbles/shaders)

set(MARBLE_SHADER_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/garden_shaders)

file(MAKE_DIRECTORY ${MARBLE_SHADER_SPV_DIR})

include(FetchContent)

set(MARBLE_SHADER_GLSL_MODULES mesh.vert mesh.frag mesh_emissive.frag)

set(MARBLE_SAMPLE_MESH_SHADERS_PACK_BASENAME "sample_mesh_shaders.marbpak")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(MARBLE_SAMPLE_SHADER_REGISTRY_PATHS_BODY "")
set(MARBLE_SAMPLE_SHADER_BASENAME_PATHS_BODY "")
foreach(_marble_shader_mod ${MARBLE_SHADER_GLSL_MODULES})
    set(_marble_spv "${_marble_shader_mod}.spv")
    string(APPEND MARBLE_SAMPLE_SHADER_REGISTRY_PATHS_BODY "    \"shaders/${_marble_spv}\",\n")
    string(APPEND MARBLE_SAMPLE_SHADER_BASENAME_PATHS_BODY "    \"${_marble_spv}\",\n")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MarbleSampleShaderNames.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/MarbleSampleShaderNames.hpp"
    @ONLY
)

set(MARBLE_SHADER_SPVS "")

foreach(NAME ${MARBLE_SHADER_GLSL_MODULES})

    set(SPV ${MARBLE_SHADER_SPV_DIR}/${NAME}.spv)

    add_custom_command(

        OUTPUT ${SPV}

        COMMAND ${MARBLE_GLSLC} ${MARBLE_GLSLC_EXTRA_FLAGS} --target-env=${MARBLE_GLSLC_TARGET_ENV} -o ${SPV} ${MARBLE_SHADER_SRC_DIR}/${NAME}

        DEPENDS ${MARBLE_SHADER_SRC_DIR}/${NAME}

        COMMENT "Compile ${NAME} -> SPIR-V"

        VERBATIM

    )

    list(APPEND MARBLE_SHADER_SPVS ${SPV})

endforeach()



add_custom_target(garden_shaders DEPENDS ${MARBLE_SHADER_SPVS})



add_library(garden_sim STATIC

    garden/GardenSimulation.cpp

    garden/GardenAuthorityTick.cpp

)



target_include_directories(garden_sim

PUBLIC

    ${CMAKE_CURRENT_SOURCE_DIR}

)



target_link_libraries(garden_sim

PUBLIC

    engine

)



add_executable(garden)



target_include_directories(garden

PRIVATE

    ${CMAKE_CURRENT_SOURCE_DIR}

    ${CMAKE_CURRENT_BINARY_DIR}/generated

)



target_sources(garden

PRIVATE

    marbles/main.cpp

    marbles/MarbleApp.cpp

    marbles/MarblesGame.cpp

    garden/GardenGame.cpp

    garden/GardenGameHelpers.cpp

    garden/GardenGameStateNetPhysics.cpp

    garden/GardenGameStateStep.cpp

    garden/GardenGameStateRender.cpp

    garden/GardenRemotePresentation.cpp

)



add_dependencies(garden garden_shaders)

add_dependencies(garden marble_pack_v1)

add_dependencies(garden marble_mesh_v1)



target_link_libraries(garden

PRIVATE

    garden_sim

)



add_custom_command(

    TARGET garden

    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory

            ${MARBLE_SHADER_SPV_DIR}

            $<TARGET_FILE_DIR:garden>/shaders

    COMMENT "Stage SPIR-V shaders next to garden"

    VERBATIM

)



add_custom_command(

    TARGET garden

    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory

            ${CMAKE_SOURCE_DIR}/assets

            $<TARGET_FILE_DIR:garden>/assets

    COMMENT "Stage runtime assets next to garden"

    VERBATIM

)



add_custom_command(

    TARGET garden

    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:garden>/assets/shaders

    COMMAND ${CMAKE_COMMAND} -E copy_directory

            ${MARBLE_SHADER_SPV_DIR}

            $<TARGET_FILE_DIR:garden>/assets/shaders

    COMMENT "Stage SPIR-V under assets/shaders for BinaryResourceManager"

    VERBATIM

)

set(MARBLE_SAMPLE_MESH_SHADER_PACK_CMD
    $<TARGET_FILE:marble_pack_v1>
    $<TARGET_FILE_DIR:garden>/assets/${MARBLE_SAMPLE_MESH_SHADERS_PACK_BASENAME}
)

foreach(_marble_pack_shader ${MARBLE_SHADER_GLSL_MODULES})

    list(APPEND MARBLE_SAMPLE_MESH_SHADER_PACK_CMD "shaders/${_marble_pack_shader}.spv" "${MARBLE_SHADER_SPV_DIR}/${_marble_pack_shader}.spv")

endforeach()

add_custom_command(

    TARGET garden

    POST_BUILD

    COMMAND ${MARBLE_SAMPLE_MESH_SHADER_PACK_CMD}

    COMMENT "Build sample_mesh_shaders.marbpak (MARBPK01) beside garden"

    VERBATIM

)

add_executable(garden_server
    garden_server/GardenServerMain.cpp
)

target_include_directories(garden_server
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(garden_server
PRIVATE
    garden_sim
)

if(TARGET marble_pack_v1)
    add_dependencies(marble_pack_v1 garden_shaders)
endif()

