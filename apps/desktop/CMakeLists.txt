message(STATUS "Configuring Local Jarvis Qt desktop target. If Qt6 is not found, set CMAKE_PREFIX_PATH or LOCAL_JARVIS_QT_PREFIX to the Qt MSVC kit directory.")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(local-jarvis-desktop
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/companion/AssistantPanelWindow.h
    src/companion/AssistantPanelWindow.cpp
    src/companion/CaptionBubbleWindow.h
    src/companion/CaptionBubbleWindow.cpp
    src/companion/CompanionWindow.h
    src/companion/CompanionWindow.cpp
)

target_include_directories(local-jarvis-desktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
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

local_jarvis_copy_whisper_runtime(local-jarvis-desktop)

if(WIN32)
    get_target_property(_local_jarvis_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_local_jarvis_qmake)
        get_filename_component(_local_jarvis_qt_bin_dir "${_local_jarvis_qmake}" DIRECTORY)
    endif()

    find_program(LOCAL_JARVIS_WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS
            "${_local_jarvis_qt_bin_dir}"
            "${Qt6_DIR}/../../../bin"
    )

    if(LOCAL_JARVIS_WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET local-jarvis-desktop POST_BUILD
            COMMAND "${LOCAL_JARVIS_WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --dir "$<TARGET_FILE_DIR:local-jarvis-desktop>"
                "$<TARGET_FILE:local-jarvis-desktop>"
            COMMENT "Deploying Qt runtime DLLs for local desktop smoke tests"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt was not found. The desktop target may build but fail to launch until Qt runtime DLLs are on PATH or copied beside local-jarvis-desktop.exe.")
    endif()
endif()
