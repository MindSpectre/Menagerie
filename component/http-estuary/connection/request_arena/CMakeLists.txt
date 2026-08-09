##############################################################################
# Http Connection — RequestArena (per-connection monotonic arena)
##############################################################################
add_library(${Estuary}.Connection.RequestArena STATIC
        request_arena.cpp
)

target_include_directories(${Estuary}.Connection.RequestArena PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Connection.RequestArena PUBLIC
        Menagerie::Common::Beaver
)
##############################################################################
