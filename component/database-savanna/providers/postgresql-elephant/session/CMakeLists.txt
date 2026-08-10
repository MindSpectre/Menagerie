add_subdirectory(shared)
add_subdirectory(free_lock)
add_subdirectory(blocking)

add_combined_library(${SAVANNA_ELEPHANT}.Session
        DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
        SOURCES
        postgres_session.hpp
        LIBRARIES
        ${SAVANNA_ELEPHANT}.Session.FreeLock
        ${SAVANNA_ELEPHANT}.Session.Blocking
)