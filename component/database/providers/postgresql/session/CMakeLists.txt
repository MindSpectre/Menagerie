add_subdirectory(shared)
add_subdirectory(free_lock)
add_subdirectory(blocking)

add_combined_library(${DATABASE_POSTGRESQL}.Session
        DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        SOURCES
        postgres_session.hpp
        LIBRARIES
        ${DATABASE_POSTGRESQL}.Session.FreeLock
        ${DATABASE_POSTGRESQL}.Session.Blocking
)