cmake_minimum_required(VERSION 3.25)

project(NBI
    VERSION 1.0.0
    DESCRIPTION "Notebook Inspector - Laptop Hardware Diagnostics"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# MSVC compiler options
# ---------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /W4          # High warning level
        /WX-         # Warnings are NOT errors (relax for now; flip to /WX when clean)
        /permissive- # Conformance mode
        /utf-8       # Source and execution charset
        /MP          # Multi-processor compilation
    )
    # Suppress a few noisy but benign MSVC warnings
    add_compile_options(/wd4251 /wd4275)
endif()

# ---------------------------------------------------------------------------
# Global preprocessor definitions
# ---------------------------------------------------------------------------
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
)

# ---------------------------------------------------------------------------
# MSVC runtime — static CRT
# ---------------------------------------------------------------------------
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ---------------------------------------------------------------------------
# Output directories
# ---------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Keep per-config subdirectories consistent
foreach(_config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config} "${CMAKE_BINARY_DIR}/lib")
endforeach()

# ---------------------------------------------------------------------------
# Qt 6
# ---------------------------------------------------------------------------
find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Multimedia PrintSupport Concurrent)

# ---------------------------------------------------------------------------
# Subprojects
# ---------------------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(gui)
