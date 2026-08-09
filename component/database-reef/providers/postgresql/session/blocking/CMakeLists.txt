##############################################################################
# PostgreSQL Blocking Session (mutex + CV + FIFO waiter pool)
##############################################################################
add_library(${REEF_POSTGRESQL}.Session.Blocking STATIC
        postgres_blocking_session.hpp
        postgres_blocking_session.cpp
        pool/blocking_pool.hpp
        pool/blocking_pool.cpp
        pool/queued_holder.hpp
        pool/queued_holder.cpp
        pool/waiter.hpp
        pool/async_waiter.hpp
)
target_include_directories(${REEF_POSTGRESQL}.Session.Blocking PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${REEF_POSTGRESQL}.Session.Blocking
        PUBLIC
        ${REEF_POSTGRESQL}.ConnectionHolder
        ${REEF_POSTGRESQL}.Session.Shared
        ${REEF_POSTGRESQL}.Transaction
        ${REEF_POSTGRESQL}.Config
        ${REEF_POSTGRESQL}.Errors
        ${REEF}.Base.Errors
        Menagerie::Common::Beaver
        PostgreSQL::PostgreSQL
)
##############################################################################
