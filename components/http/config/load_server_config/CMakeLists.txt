##############################################################################
# Http Config — load_server_config / dump_server_config + config error types
##############################################################################
add_library(${HTTP}.Config.LoadServerConfig STATIC
        load_server_config.cpp
        load_server_config.hpp
)

target_include_directories(${HTTP}.Config.LoadServerConfig PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Config.LoadServerConfig
        PUBLIC
        ${HTTP}.Config.ServerConfig
        ${HTTP}.Types
        Menagerie::Common::Beavers
        Menagerie::Common::Serialization
        JsonCpp::JsonCpp
)
##############################################################################
