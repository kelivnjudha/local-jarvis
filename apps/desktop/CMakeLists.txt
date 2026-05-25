find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(local-jarvis-desktop
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
)

target_link_libraries(local-jarvis-desktop
    PRIVATE
        Qt6::Widgets
        local_jarvis_core
)

set_target_properties(local-jarvis-desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
