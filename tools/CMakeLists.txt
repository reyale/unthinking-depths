add_executable(gen_example_replay gen_replay.cpp)
target_link_libraries(gen_example_replay PRIVATE engine)
target_include_directories(gen_example_replay PRIVATE ${CMAKE_SOURCE_DIR}/fixtures)

add_executable(sfbg_mapgen sfbg_mapgen.cpp)
target_link_libraries(sfbg_mapgen PRIVATE maps_lib)

add_executable(sfbg_mapcheck sfbg_mapcheck.cpp)
target_link_libraries(sfbg_mapcheck PRIVATE maps_lib)
