add_subdirectory(shared)
add_subdirectory(free_lock)
add_subdirectory(blocking)

add_combined_library(${REEF_POSTGRESQL}.Session
        DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        SOURCES
        postgres_session.hpp
        LIBRARIES
        ${REEF_POSTGRESQL}.Session.FreeLock
        ${REEF_POSTGRESQL}.Session.Blocking
)