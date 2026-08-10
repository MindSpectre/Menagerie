##############################################################################
# Http Connection — RequestArena (per-connection monotonic arena)
##############################################################################
add_library(${Albatross}.Connection.RequestArena STATIC
        request_arena.cpp
)

target_include_directories(${Albatross}.Connection.RequestArena PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Connection.RequestArena PUBLIC
        Menagerie::Common::Beaver
)
##############################################################################
