cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# "core" — static library containing hardware scan logic
# ---------------------------------------------------------------------------
add_library(core STATIC
    auto_scan.cpp
    storage_manager.cpp
)

target_include_directories(core
    PUBLIC
        ${CMAKE_SOURCE_DIR}   # allows #include "core/auto_scan.hpp"
)

target_link_libraries(core
    PUBLIC
        common
        Qt6::Core
)

# WMI link dependency (Windows only)
if(WIN32)
    target_link_libraries(core PUBLIC wbemuuid)
endif()
