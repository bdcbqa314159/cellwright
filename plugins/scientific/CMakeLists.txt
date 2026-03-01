add_library(scientific SHARED
    scientific.cpp
    ${CMAKE_SOURCE_DIR}/src/core/CellValue.cpp
)

target_link_libraries(scientific PRIVATE plugin_arch)

target_include_directories(scientific PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(scientific PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
