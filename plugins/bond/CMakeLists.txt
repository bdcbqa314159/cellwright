add_library(bond_plugin SHARED
    bond_plugin.cpp
    bond_register.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CellValue.cpp
)

target_link_libraries(bond_plugin PRIVATE plugin_arch)

target_include_directories(bond_plugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(bond_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
