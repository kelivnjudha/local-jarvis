cmake_minimum_required(VERSION 3.22)

project(LocalJarvis
    VERSION 0.1.0
    DESCRIPTION "Local-first privacy-focused desktop AI assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOCAL_JARVIS_BUILD_TESTS "Build Local Jarvis tests" ON)
option(LOCAL_JARVIS_BUILD_DESKTOP "Build the Qt desktop application" ON)
option(LOCAL_JARVIS_ENABLE_WHISPER "Enable local whisper.cpp ASR integration hooks" OFF)

set(LOCAL_JARVIS_WHISPER_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp"
    CACHE PATH
    "Path to a local whisper.cpp checkout"
)

find_package(SQLite3 REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(LOCAL_JARVIS_CORE_SOURCES
    core/caption/CaptionFormatter.cpp
    core/caption/CaptionManager.cpp
    core/caption/DummyCaptionSource.cpp
    core/session/SessionManager.cpp
    core/companion/AnimationStateMachine.cpp
    core/companion/CompanionAssetRegistry.cpp
    core/companion/CompanionManager.cpp
    core/companion/CompanionTheme.cpp
    core/companion/CompanionVisualProfile.cpp
    core/ai/OllamaClient.cpp
    core/ai/ModelManager.cpp
    core/ai/PromptBuilder.cpp
    core/processing/ProcessingJob.cpp
    core/processing/ProcessingQueue.cpp
    core/processing/StudyNoteProcessor.cpp
    core/processing/MeetingNoteProcessor.cpp
    core/processing/JsonRepair.cpp
    core/setup/SetupManager.cpp
    core/setup/SystemCheck.cpp
    core/audio/AudioCapture.cpp
    core/audio/AudioLevelMeter.cpp
    core/audio/DummyAudioCapture.cpp
    core/audio/MicrophoneCapture.cpp
    core/asr/AudioChunkBuffer.cpp
    core/screen/ScreenCapture.cpp
    core/asr/AsrEngine.cpp
    core/asr/AsrEngineFactory.cpp
    core/asr/AsrWorker.cpp
    core/ocr/OcrEngine.cpp
    core/llm/LlmEngine.cpp
    core/storage/Storage.cpp
    core/privacy/PrivacyManager.cpp
    core/logging/Logger.cpp
)

if(LOCAL_JARVIS_ENABLE_WHISPER)
    list(APPEND LOCAL_JARVIS_CORE_SOURCES
        core/asr/WhisperAsrEngine.cpp
    )

    if(NOT EXISTS "${LOCAL_JARVIS_WHISPER_SOURCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "LOCAL_JARVIS_ENABLE_WHISPER is ON, but whisper.cpp was not found at ${LOCAL_JARVIS_WHISPER_SOURCE_DIR}. Set LOCAL_JARVIS_WHISPER_SOURCE_DIR to a local whisper.cpp checkout or configure with LOCAL_JARVIS_ENABLE_WHISPER=OFF.")
    endif()

    add_subdirectory("${LOCAL_JARVIS_WHISPER_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/third_party/whisper.cpp" EXCLUDE_FROM_ALL)
    set(LOCAL_JARVIS_WHISPER_AVAILABLE TRUE)
else()
    list(APPEND LOCAL_JARVIS_CORE_SOURCES
        core/asr/StubAsrEngine.cpp
    )
endif()

if(WIN32)
    list(APPEND LOCAL_JARVIS_CORE_SOURCES
        core/audio/AudioCapture_win.cpp
        core/audio/MicrophoneCapture_Windows.cpp
        core/screen/ScreenCapture_win.cpp
    )
elseif(APPLE)
    list(APPEND LOCAL_JARVIS_CORE_SOURCES
        core/audio/AudioCapture_macos.cpp
        core/screen/ScreenCapture_macos.cpp
    )
elseif(UNIX)
    list(APPEND LOCAL_JARVIS_CORE_SOURCES
        core/audio/AudioCapture_linux.cpp
        core/screen/ScreenCapture_linux.cpp
    )
endif()

add_library(local_jarvis_core STATIC ${LOCAL_JARVIS_CORE_SOURCES})

target_include_directories(local_jarvis_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(local_jarvis_core
    PUBLIC
        SQLite::SQLite3
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(local_jarvis_core PRIVATE ws2_32 ole32 propsys uuid)
endif()

target_compile_definitions(local_jarvis_core
    PUBLIC
        LOCAL_JARVIS_ENABLE_WHISPER=$<BOOL:${LOCAL_JARVIS_ENABLE_WHISPER}>
)

if(LOCAL_JARVIS_ENABLE_WHISPER AND LOCAL_JARVIS_WHISPER_AVAILABLE AND TARGET whisper)
    target_link_libraries(local_jarvis_core PRIVATE whisper)
    target_compile_definitions(local_jarvis_core PRIVATE LOCAL_JARVIS_WHISPER_LINKED=1)
else()
    if(LOCAL_JARVIS_ENABLE_WHISPER AND LOCAL_JARVIS_WHISPER_AVAILABLE AND NOT TARGET whisper)
        message(FATAL_ERROR "LOCAL_JARVIS_ENABLE_WHISPER is ON and whisper.cpp was found, but no CMake target named 'whisper' was exported.")
    endif()
    target_compile_definitions(local_jarvis_core PRIVATE LOCAL_JARVIS_WHISPER_LINKED=0)
endif()

if(WIN32)
    target_compile_definitions(local_jarvis_core PUBLIC LOCAL_JARVIS_PLATFORM_WINDOWS=1)
elseif(APPLE)
    target_compile_definitions(local_jarvis_core PUBLIC LOCAL_JARVIS_PLATFORM_MACOS=1)
elseif(UNIX)
    target_compile_definitions(local_jarvis_core PUBLIC LOCAL_JARVIS_PLATFORM_LINUX=1)
endif()

if(LOCAL_JARVIS_BUILD_DESKTOP)
    add_subdirectory(apps/desktop)
endif()

if(LOCAL_JARVIS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
