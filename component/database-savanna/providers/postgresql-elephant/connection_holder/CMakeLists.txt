##############################################################################
# PostgreSql ConnectionHolder (INTERFACE — shared by executors and the pool)
##############################################################################
add_library(${SAVANNA_ELEPHANT}.ConnectionHolder INTERFACE
        connection.hpp
        connection_holder.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.ConnectionHolder INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.ConnectionHolder INTERFACE
        PostgreSQL::PostgreSQL
)
##############################################################################
