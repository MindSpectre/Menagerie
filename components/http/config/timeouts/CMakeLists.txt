##############################################################################
# Http Config — Timeouts (per-phase timeout set)
##############################################################################
add_library(${HTTP}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${HTTP}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Config.Timeouts INTERFACE
        Menagerie::Common::Serialization
)
##############################################################################
