add_executable(unit_tests
    test_core.cpp
)

target_link_libraries(unit_tests PRIVATE recorder_core)

add_test(NAME CoreUnitTests COMMAND unit_tests)
