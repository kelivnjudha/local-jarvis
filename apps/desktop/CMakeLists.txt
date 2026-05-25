message(STATUS "Configuring Local Jarvis Qt desktop target. If Qt6 is not found, set CMAKE_PREFIX_PATH or LOCAL_JARVIS_QT_PREFIX to the Qt MSVC kit directory.")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(local-jarvis-desktop
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(local-jarvis-desktop
    PRIVATE
        local_jarvis_core
        Qt6::Widgets
)

set_target_properties(local-jarvis-desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
