##############################################################################
# Http Connection — RequestArena (per-connection monotonic arena)
##############################################################################
add_library(${Http}.Connection.RequestArena STATIC
        request_arena.cpp
)

target_include_directories(${Http}.Connection.RequestArena PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Connection.RequestArena PUBLIC
        Menagerie::Common::Beavers
)
##############################################################################
