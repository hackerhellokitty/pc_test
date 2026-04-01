cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# Qt setup for this directory
# ---------------------------------------------------------------------------
qt_standard_project_setup()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF)   # We build UI manually — no .ui files in Phase 1

# ---------------------------------------------------------------------------
# NBI executable
# ---------------------------------------------------------------------------
qt_add_executable(NBI
    main.cpp
    main_dashboard.cpp
    main_dashboard.hpp
    keyboard_view.cpp
    keyboard_view.hpp
    keyboard_window.cpp
    keyboard_window.hpp
    screen_test_view.cpp
    screen_test_view.hpp
    screen_window.cpp
    screen_window.hpp
    touch_canvas_view.cpp
    touch_canvas_view.hpp
    storage_view.cpp
    storage_view.hpp
    storage_window.cpp
    storage_window.hpp
    network_view.cpp
    network_view.hpp
    network_window.cpp
    network_window.hpp
    audio_view.cpp
    audio_view.hpp
    audio_window.cpp
    audio_window.hpp
    battery_view.cpp
    battery_view.hpp
    battery_window.cpp
    battery_window.hpp
    physical_checklist_view.cpp
    physical_checklist_view.hpp
    physical_checklist_window.cpp
    physical_checklist_window.hpp
    thermal_view.cpp
    thermal_view.hpp
    thermal_window.cpp
    thermal_window.hpp
    touchpad_view.cpp
    touchpad_view.hpp
    touchpad_window.cpp
    touchpad_window.hpp
    gesture_test_widget.cpp
    gesture_test_widget.hpp
    mic_view.cpp
    mic_view.hpp
    mic_window.cpp
    mic_window.hpp
    ports_view.cpp
    ports_view.hpp
    ports_window.cpp
    ports_window.hpp
    summary_page.cpp
    summary_page.hpp
    summary_window.cpp
    summary_window.hpp
    report_generator.cpp
    report_generator.hpp
    resources/nbi.qrc
    # Win32 resource script: embeds UAC manifest + version info
    ${CMAKE_SOURCE_DIR}/nbi.rc
)

set_target_properties(NBI PROPERTIES
    WIN32_EXECUTABLE TRUE          # Subsystem WINDOWS (no console)
    OUTPUT_NAME "NotebookInspector"
    AUTOGEN_TARGET_DEPENDS ""
)

# Disable MSVC's auto-generated manifest — we embed our own via nbi.rc
if(MSVC)
    target_link_options(NBI PRIVATE "/MANIFEST:NO")
endif()

# Make "gui/" a known include root so #include "gui/main_dashboard.hpp" works
target_include_directories(NBI
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# ---------------------------------------------------------------------------
# Link dependencies
# ---------------------------------------------------------------------------
target_link_libraries(NBI
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::PrintSupport
        Qt6::Concurrent
        common
        core
        Dxva2          # DDC/CI: GetMonitorBrightness / SetMonitorBrightness
)

# ---------------------------------------------------------------------------
# Post-build: deploy Qt DLLs next to the executable (development mode)
# windeployqt is located alongside qmake in the Qt bin directory.
# ---------------------------------------------------------------------------
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt6
        HINTS
            "${Qt6_DIR}/../../../bin"
            "$ENV{Qt6_DIR}/../../../bin"
        DOC "Path to windeployqt"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET NBI POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-compiler-runtime
                    --release
                    "$<TARGET_FILE:NBI>"
            COMMENT "Running windeployqt on NotebookInspector.exe"
            VERBATIM
        )
    else()
        message(STATUS
            "windeployqt not found — skipping Qt deployment step. "
            "Set Qt6_DIR or add Qt bin to PATH.")
    endif()
endif()
