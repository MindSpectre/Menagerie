##############################################################################
# PostgreSQL Free-Lock Session (ring buffer pool, fail-fast acquire)
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Session.FreeLock STATIC
        postgres_free_lock_session.hpp
        postgres_free_lock_session.cpp
        pool/connection_pool.hpp
        pool/connection_pool.cpp
        pool/slot_holder.hpp
        pool/pool_janitor.hpp
        pool/pool_janitor.cpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Session.FreeLock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${DATABASE_POSTGRESQL}.Session.FreeLock
        PUBLIC
        ${DATABASE_POSTGRESQL}.ConnectionHolder
        ${DATABASE_POSTGRESQL}.Session.Shared
        ${DATABASE_POSTGRESQL}.Config
        ${DATABASE_POSTGRESQL}.Errors
        ${DATABASE_POSTGRESQL}.Transaction
        ${DATABASE}.Base.Errors
        Menagerie::Common::Beavers
        Menagerie::Common::Serialization
        Menagerie::Common::Multithread
        PostgreSQL::PostgreSQL
)
##############################################################################
