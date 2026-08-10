##############################################################################
# PostgreSql Config
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Config STATIC
        credentials/postgres_connection_credentials.hpp
        credentials/postgres_connection_credentials.cpp
        postgres_connection_config.hpp
        postgres_connection_config.cpp
        tools/postgres_ssl_mode.hpp
        tools/postgres_nodes_role.hpp
        tools/postgres_connection_tools.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Config PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.Config
        PUBLIC
        ${SAVANNA_ELEPHANT}.Registries
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
        JsonCpp::JsonCpp
)
##############################################################################