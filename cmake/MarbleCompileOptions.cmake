# Marble-first compile/link conventions (see docs/decisions/ADR-0006-toolchain-compile-policy.md).
# Applied to our targets only; dependencies (e.g. GLFW) keep their own flags.

add_library(marble_compile_options INTERFACE)
add_library(marble::compile_options ALIAS marble_compile_options)

if(MSVC)
    target_compile_options(marble_compile_options INTERFACE
        /W4
        /permissive-
    )
else()
    target_compile_options(marble_compile_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# Multi-config generators (Visual Studio): per-config definitions.
target_compile_definitions(marble_compile_options INTERFACE
    $<$<CONFIG:Debug>:MARBLE_DEBUG=1>
)
