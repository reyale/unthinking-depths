add_library(runner_lib STATIC wasm_bot.cpp)

target_include_directories(runner_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(runner_lib PUBLIC engine wasmtime_lib)
