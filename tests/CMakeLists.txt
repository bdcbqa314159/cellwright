add_executable(cellwright_tests
    test_arena.cpp
    test_arrow_bridge.cpp
    test_async_recalc.cpp
    test_bond_plugin.cpp
    test_cell_address.cpp
    test_clipboard.cpp
    test_column.cpp
    test_command.cpp
    test_csv.cpp
    test_date_serial.cpp
    test_dependency_graph.cpp
    test_duckdb_engine.cpp
    test_evaluator.cpp
    test_format.cpp
    test_framebuffer_rect.cpp
    test_hot_reload.cpp
    test_parser.cpp
    test_plugin_allowlist.cpp
    test_plugin_interface.cpp
    test_python_plugin.cpp
    test_row_col_ops.cpp
    test_scientific_plugin.cpp
    test_sheet.cpp
    test_simd_ops.cpp
    test_sort.cpp
    test_text_functions.cpp
    test_tokenizer.cpp
    test_workbook_io.cpp
    # Plugin sources for direct-instantiation tests
    ${CMAKE_SOURCE_DIR}/plugins/scientific/scientific.cpp
    ${CMAKE_SOURCE_DIR}/plugins/bond/bond_plugin.cpp
)

target_compile_definitions(cellwright_tests PRIVATE
    PY_BOND_PLUGIN_PATH="${CMAKE_SOURCE_DIR}/plugins/py_bond/py_bond.py"
)

target_link_libraries(cellwright_tests PRIVATE
    cellwright_core
    GTest::gtest_main
)

include(GoogleTest)
# PRE_TEST: enumerate tests when ctest runs, not during the build. The default
# POST_BUILD mode executes the binary at build time with a 5s timeout, which makes
# the BUILD fail if the binary is merely slow to start — and it is: it loads
# libpython plus a 38MB libduckdb. Observed failing on a loaded machine mid-listing,
# and it is why an early dyld fault failed the build instead of the tests.
gtest_discover_tests(cellwright_tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 60
)

cellwright_set_warnings(cellwright_tests)
