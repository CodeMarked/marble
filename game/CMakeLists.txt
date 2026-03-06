add_executable(marbles)

target_sources(marbles
PRIVATE
    marbles/main.cpp
)

target_link_libraries(marbles
PRIVATE
    engine
)
