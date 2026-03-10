# Collect the source files we need (everything except main.cpp)
file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_SOURCE_DIR}/src/formula/*.cpp
    ${CMAKE_SOURCE_DIR}/src/builtin/*.cpp
    ${CMAKE_SOURCE_DIR}/src/io/*.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginManager.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PluginAllowlist.cpp
    ${CMAKE_SOURCE_DIR}/src/plugin/PyFunctionPlugin.cpp
)

# Test sources
file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

# Plugin sources needed for direct-instantiation tests
set(PLUGIN_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/plugins/scientific/scientific.cpp
    ${CMAKE_SOURCE_DIR}/plugins/bond/bond_plugin.cpp
)

add_executable(cellwright_tests ${TEST_SOURCES} ${LIB_SOURCES} ${PLUGIN_TEST_SOURCES})

target_include_directories(cellwright_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# SIMD flags for x86_64
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_options(cellwright_tests PRIVATE -mavx2 -mfma)
endif()

target_compile_definitions(cellwright_tests PRIVATE
    PY_BOND_PLUGIN_PATH="${CMAKE_SOURCE_DIR}/plugins/py_bond/py_bond.py"
)

target_link_libraries(cellwright_tests PRIVATE
    GTest::gtest_main
    plugin_arch
    pybind11::embed
    duckdb
)

include(GoogleTest)
gtest_discover_tests(cellwright_tests)
