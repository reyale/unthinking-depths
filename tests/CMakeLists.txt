add_executable(engine_tests
    test_rng.cpp
    test_ids.cpp
    test_world.cpp
    test_determinism.cpp
    test_combat.cpp
    test_territory.cpp
    test_renderer.cpp
)

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/fixtures
    ${CMAKE_SOURCE_DIR}/viz
)

target_link_libraries(engine_tests PRIVATE engine viz_lib GTest::gtest_main)

add_executable(runner_tests
    test_wasm_runner.cpp
)

target_include_directories(runner_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/runner
)

target_link_libraries(runner_tests PRIVATE runner_lib GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(engine_tests)
gtest_discover_tests(runner_tests)
