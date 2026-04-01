cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# "core" — static library containing hardware scan logic
# ---------------------------------------------------------------------------
add_library(core STATIC
    auto_scan.cpp
    storage_manager.cpp
    network_manager.cpp
    audio_manager.cpp
    battery_manager.cpp
    thermal_manager.cpp
    scorer.cpp
    scorer.hpp
)

target_include_directories(core
    PUBLIC
        ${CMAKE_SOURCE_DIR}   # allows #include "core/auto_scan.hpp"
)

target_link_libraries(core
    PUBLIC
        common
        Qt6::Core
        Qt6::Multimedia
)

# WMI link dependency (Windows only)
if(WIN32)
    target_link_libraries(core PUBLIC wbemuuid ws2_32 iphlpapi wlanapi)
endif()
