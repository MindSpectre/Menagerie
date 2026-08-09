##############################################################################
# Http Config — Timeouts (per-phase timeout set)
##############################################################################
add_library(${Albatross}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${Albatross}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Config.Timeouts INTERFACE
        Menagerie::Common::Pangolin
)
##############################################################################
