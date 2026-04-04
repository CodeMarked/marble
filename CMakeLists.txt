cmake_minimum_required(VERSION 3.20)

project(Marble)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MarbleCompileOptions.cmake")

include(CTest)

add_subdirectory(engine)
add_subdirectory(game)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
