add_executable(engine_tests
    test_rng.cpp
    test_ids.cpp
    test_world.cpp
    test_determinism.cpp
    test_combat.cpp
)

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/fixtures
)

target_link_libraries(engine_tests PRIVATE engine GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(engine_tests)
