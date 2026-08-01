##############################################################################
# Http Config — Timeouts (per-phase timeout set)
##############################################################################
add_library(${Http}.Config.Timeouts INTERFACE timeouts.hpp)

target_include_directories(${Http}.Config.Timeouts INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Config.Timeouts INTERFACE
        Menagerie::Common::Serialization
)
##############################################################################
