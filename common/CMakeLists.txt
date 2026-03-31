cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# "common" — header-only INTERFACE library
# Consumers link against this target to get types.hpp and Qt6::Core.
# ---------------------------------------------------------------------------
add_library(common INTERFACE)

target_include_directories(common
    INTERFACE
        ${CMAKE_SOURCE_DIR}   # allows #include "common/types.hpp"
)

target_link_libraries(common
    INTERFACE
        Qt6::Core
)
