add_executable(local_jarvis_core_smoke_test
    CoreSmokeTest.cpp
)

target_link_libraries(local_jarvis_core_smoke_test
    PRIVATE
        local_jarvis_core
)

add_test(NAME local_jarvis_core_smoke_test COMMAND local_jarvis_core_smoke_test)
