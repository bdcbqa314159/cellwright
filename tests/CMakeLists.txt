# Collect the source files we need (everything except main.cpp)
file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_SOURCE_DIR}/src/formula/*.cpp
    ${CMAKE_SOURCE_DIR}/src/builtin/*.cpp
    ${CMAKE_SOURCE_DIR}/src/io/*.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginAllowlist.cpp
)

# Test sources
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

# Plugin sources needed for direct-instantiation tests
set(PLUGIN_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/plugins/scientific/scientific.cpp
    ${CMAKE_SOURCE_DIR}/plugins/bond/bond_plugin.cpp
)

add_executable(magic_tests ${TEST_SOURCES} ${LIB_SOURCES} ${PLUGIN_TEST_SOURCES})

target_include_directories(magic_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(magic_tests PRIVATE
    GTest::gtest_main
    plugin_arch
)

include(GoogleTest)
gtest_discover_tests(magic_tests)
