##############################################################################
# PostgreSQL Free-Lock Session (ring buffer pool, fail-fast acquire)
##############################################################################
add_library(${REEF_POSTGRESQL}.Session.FreeLock STATIC
        postgres_free_lock_session.hpp
        postgres_free_lock_session.cpp
        pool/connection_pool.hpp
        pool/connection_pool.cpp
        pool/slot_holder.hpp
        pool/pool_janitor.hpp
        pool/pool_janitor.cpp
)
target_include_directories(${REEF_POSTGRESQL}.Session.FreeLock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${REEF_POSTGRESQL}.Session.FreeLock
        PUBLIC
        ${REEF_POSTGRESQL}.ConnectionHolder
        ${REEF_POSTGRESQL}.Session.Shared
        ${REEF_POSTGRESQL}.Config
        ${REEF_POSTGRESQL}.Errors
        ${REEF_POSTGRESQL}.Transaction
        ${REEF}.Base.Errors
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
        Menagerie::Common::Starling
        PostgreSQL::PostgreSQL
)
##############################################################################
