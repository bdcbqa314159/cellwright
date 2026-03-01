add_library(example_stats SHARED example_stats.cpp)

target_link_libraries(example_stats PRIVATE plugin_arch)

target_include_directories(example_stats PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(example_stats PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
