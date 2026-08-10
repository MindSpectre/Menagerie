##############################################################################
# PostgreSQL Free-Lock Session (ring buffer pool, fail-fast acquire)
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Session.FreeLock STATIC
        postgres_free_lock_session.hpp
        postgres_free_lock_session.cpp
        pool/connection_pool.hpp
        pool/connection_pool.cpp
        pool/slot_holder.hpp
        pool/pool_janitor.hpp
        pool/pool_janitor.cpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Session.FreeLock PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${SAVANNA_ELEPHANT}.Session.FreeLock
        PUBLIC
        ${SAVANNA_ELEPHANT}.ConnectionHolder
        ${SAVANNA_ELEPHANT}.Session.Shared
        ${SAVANNA_ELEPHANT}.Config
        ${SAVANNA_ELEPHANT}.Errors
        ${SAVANNA_ELEPHANT}.Transaction
        ${SAVANNA}.Base.Errors
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
        Menagerie::Common::Starling
        PostgreSQL::PostgreSQL
)
##############################################################################
