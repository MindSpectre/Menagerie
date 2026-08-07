##############################################################################
# PostgreSQL Blocking Session (mutex + CV + FIFO waiter pool)
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Session.Blocking STATIC
        postgres_blocking_session.hpp
        postgres_blocking_session.cpp
        pool/blocking_pool.hpp
        pool/blocking_pool.cpp
        pool/queued_holder.hpp
        pool/queued_holder.cpp
        pool/waiter.hpp
        pool/async_waiter.hpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Session.Blocking PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${DATABASE_POSTGRESQL}.Session.Blocking
        PUBLIC
        ${DATABASE_POSTGRESQL}.ConnectionHolder
        ${DATABASE_POSTGRESQL}.Session.Shared
        ${DATABASE_POSTGRESQL}.Transaction
        ${DATABASE_POSTGRESQL}.Config
        ${DATABASE_POSTGRESQL}.Errors
        ${DATABASE}.Base.Errors
        Menagerie::Common::Beavers
        PostgreSQL::PostgreSQL
)
##############################################################################
