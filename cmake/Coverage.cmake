if(NOT CMAKE_BUILD_TYPE STREQUAL "Coverage")
    return()
endif()

find_program(GCOVR gcovr)
if(NOT GCOVR)
    message(STATUS "gcovr not found — install with: pip install gcovr")
    message(STATUS "  Then re-run cmake to enable the 'coverage' target")
    return()
endif()

set(COVERAGE_DIR ${CMAKE_BINARY_DIR}/coverage)

add_custom_target(coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_DIR}
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -j4
    COMMAND ${GCOVR}
        --root          ${CMAKE_SOURCE_DIR}
        --filter        ".*engine/src/.*"
        --filter        ".*maps/map.*"
        --exclude       ".*abi\\.hpp"
        --html-details  ${COVERAGE_DIR}/index.html
        --xml           ${COVERAGE_DIR}/coverage.xml
        --print-summary
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Coverage report → ${COVERAGE_DIR}/index.html"
    VERBATIM
)
