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
gtest_discover_tests(cellwright_tests)
