add_library(viz_lib STATIC render.cpp ascii_out.cpp)
target_link_libraries(viz_lib PUBLIC engine)
target_include_directories(viz_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(ud_viz main.cpp)
target_link_libraries(ud_viz PRIVATE viz_lib)

find_package(Curses QUIET)
if(CURSES_FOUND)
  add_library(ncurses_out STATIC ncurses_out.cpp)
  target_link_libraries(ncurses_out PUBLIC viz_lib ${CURSES_LIBRARIES})
  target_include_directories(ncurses_out PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CURSES_INCLUDE_DIRS})

  add_executable(ud_viz_nc main_nc.cpp)
  target_link_libraries(ud_viz_nc PRIVATE ncurses_out)
else()
  message(STATUS "ncurses not found — ud_viz_nc will not be built")
endif()
