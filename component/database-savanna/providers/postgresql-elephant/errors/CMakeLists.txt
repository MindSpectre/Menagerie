##############################################################################
# PostgreSQL Error Handling Library
#
# Provides comprehensive PostgreSQL error mapping and extraction utilities.
# Changed from INTERFACE to STATIC to compile large SQLSTATE mapping functions.
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Errors STATIC
        postgres_errors.hpp
        postgres_errors.cpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Errors PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA_ELEPHANT}.Errors PUBLIC
        ${SAVANNA}.Base.Errors
        PostgreSQL::PostgreSQL
)
##############################################################################