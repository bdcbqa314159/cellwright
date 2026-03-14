add_plugin(example_stats
    SOURCES example_stats/example_stats.cpp
            ${CMAKE_SOURCE_DIR}/src/core/CellValue.cpp
    INTERFACE_DIRS ${CMAKE_SOURCE_DIR}/src
)

add_plugin(scientific
    SOURCES scientific/scientific.cpp
            ${CMAKE_SOURCE_DIR}/src/core/CellValue.cpp
    INTERFACE_DIRS ${CMAKE_SOURCE_DIR}/src
)

add_plugin(bond_plugin
    SOURCES bond/bond_plugin.cpp
            bond/bond_register.cpp
            ${CMAKE_SOURCE_DIR}/src/core/CellValue.cpp
    INTERFACE_DIRS ${CMAKE_SOURCE_DIR}/src
)
