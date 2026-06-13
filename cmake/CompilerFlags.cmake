if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        # Determinism requirements: no float contraction, no fast-math shortcuts
        -ffp-contract=off
        -fno-fast-math
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-flto=thin)
            add_link_options(-flto=thin)
        else()
            add_compile_options(-flto)
            add_link_options(-flto)
        endif()
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-O0 -g -fsanitize=address,undefined)
        add_link_options(-fsanitize=address,undefined)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            add_link_options(-static-libasan -static-libubsan)
        else()
            add_link_options(-static-libsan)
        endif()
    endif()

    if(NOT APPLE)
        add_link_options(-static-libgcc -static-libstdc++)
    endif()
endif()
