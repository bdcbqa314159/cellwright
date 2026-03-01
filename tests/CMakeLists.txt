# Collect the source files we need (everything except main.cpp)
file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_SOURCE_DIR}/src/formula/*.cpp
    ${CMAKE_SOURCE_DIR}/src/builtin/*.cpp
    ${CMAKE_SOURCE_DIR}/src/io/*.cpp
)

# Test sources
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(magic_tests ${TEST_SOURCES} ${LIB_SOURCES})

target_include_directories(magic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(magic_tests PRIVATE
    GTest::gtest_main
    plugin_arch
)

include(GoogleTest)
gtest_discover_tests(magic_tests)
