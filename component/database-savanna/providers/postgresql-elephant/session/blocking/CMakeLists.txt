##############################################################################
# PostgreSQL Blocking Session (mutex + CV + FIFO waiter pool)
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Session.Blocking STATIC
        postgres_blocking_session.hpp
        postgres_blocking_session.cpp
        pool/blocking_pool.hpp
        pool/blocking_pool.cpp
        pool/queued_holder.hpp
        pool/queued_holder.cpp
        pool/waiter.hpp
        pool/async_waiter.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Session.Blocking PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${SAVANNA_ELEPHANT}.Session.Blocking
        PUBLIC
        ${SAVANNA_ELEPHANT}.ConnectionHolder
        ${SAVANNA_ELEPHANT}.Session.Shared
        ${SAVANNA_ELEPHANT}.Transaction
        ${SAVANNA_ELEPHANT}.Config
        ${SAVANNA_ELEPHANT}.Errors
        ${SAVANNA}.Base.Errors
        Menagerie::Common::Beaver
        PostgreSQL::PostgreSQL
)
##############################################################################
