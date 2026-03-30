add_executable(marbles)

target_sources(marbles
PRIVATE
    marbles/main.cpp
)

target_link_libraries(marbles
PRIVATE
    engine
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
