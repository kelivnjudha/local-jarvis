add_executable(local_jarvis_core_smoke_test
    CoreSmokeTest.cpp
)

target_link_libraries(local_jarvis_core_smoke_test
    PRIVATE
        local_jarvis_core
)

add_test(NAME local_jarvis_core_smoke_test COMMAND local_jarvis_core_smoke_test)

add_executable(local_jarvis_processing_test
    ProcessingTest.cpp
)

target_link_libraries(local_jarvis_processing_test
    PRIVATE
        local_jarvis_core
)

add_test(NAME local_jarvis_processing_test COMMAND local_jarvis_processing_test)

add_executable(local_jarvis_companion_core_test
    CompanionCoreTest.cpp
)

target_link_libraries(local_jarvis_companion_core_test
    PRIVATE
        local_jarvis_core
)

add_test(NAME local_jarvis_companion_core_test COMMAND local_jarvis_companion_core_test)

add_executable(local_jarvis_caption_test
    CaptionTest.cpp
)

target_link_libraries(local_jarvis_caption_test
    PRIVATE
        local_jarvis_core
)

add_test(NAME local_jarvis_caption_test COMMAND local_jarvis_caption_test)
