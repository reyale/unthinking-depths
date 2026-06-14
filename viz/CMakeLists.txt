add_library(viz_lib STATIC render.cpp ascii_out.cpp)
target_link_libraries(viz_lib PUBLIC engine)
target_include_directories(viz_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(sfbg_viz main.cpp)
target_link_libraries(sfbg_viz PRIVATE viz_lib)
