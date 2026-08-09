##############################################################################
# Http Config — Timeouts (per-phase timeout set)
##############################################################################
add_library(${Estuary}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${Estuary}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Config.Timeouts INTERFACE
        Menagerie::Common::Pangolin
)
##############################################################################
