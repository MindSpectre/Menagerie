set(Http ${COMPONENT}.Http)


##############################################################################
# Http Types layer (new, PR 1 of redesign)
##############################################################################
add_subdirectory(types)
##############################################################################


##############################################################################
# Http Routing layer (PR 2 of redesign)
##############################################################################
add_subdirectory(routing)
##############################################################################


##############################################################################
# Http Connection layer (PR 3 of redesign)
##############################################################################
add_subdirectory(connection)
##############################################################################


##############################################################################
# Http Drivers layer (PR 3 of redesign)
##############################################################################
add_subdirectory(drivers)
##############################################################################


##############################################################################
# Http Config layer (PR 6 of redesign)
##############################################################################
add_subdirectory(config)
##############################################################################


##############################################################################
# Http Listeners layer (PR 4 of redesign)
##############################################################################
add_subdirectory(listeners)
##############################################################################


##############################################################################
# Http Server layer (PR 5 of redesign)
##############################################################################
add_subdirectory(server)
##############################################################################


##############################################################################
# Http Exported library
##############################################################################
add_combined_library(${Http}
        DIRECTORIES
        export/
        SOURCES
        export/menagerie/http
        LIBRARIES
        ${Http}.Types
        ${Http}.Routing
        ${Http}.Connection
        ${Http}.Drivers
        ${Http}.Listeners
        ${Http}.Config
        ${Http}.Server
)

# Asio's thread_info_base is sized by this macro (max_mem_index = 6 * cache_size)
# and lives inside io_context. A consumer TU that compiles it at the default 2
# against ours gets a differently-laid-out struct — silent corruption. The root
# CMakeLists sets it tree-wide; this re-exports it to anyone linking the umbrella.
# Both read ASIO_RECYCLE_CACHE, so they cannot drift.
target_compile_definitions(${Http} INTERFACE
        BOOST_ASIO_RECYCLING_ALLOCATOR_CACHE_SIZE=${ASIO_RECYCLE_CACHE}
)

add_library(Menagerie::Component::Http ALIAS ${Http})
##############################################################################