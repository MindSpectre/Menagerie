add_subdirectory(shared)

##############################################################################
# PostgreSQL Session (mutex + CV pool, one FIFO queue for sync and async waiters)
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Session STATIC
        postgres_session.hpp
        postgres_session.cpp
        pool/connection_pool.hpp
        pool/connection_pool.cpp
        pool/queued_holder.hpp
        pool/queued_holder.cpp
        pool/waiter.hpp
        pool/async_waiter.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Session PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/pool
)
target_link_libraries(${SAVANNA_ELEPHANT}.Session
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
