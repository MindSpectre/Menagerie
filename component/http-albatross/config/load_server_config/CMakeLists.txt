##############################################################################
# Http Config — load_server_config / dump_server_config + config error types
##############################################################################
add_library(${Albatross}.Config.LoadServerConfig STATIC
        load_server_config.cpp
        load_server_config.hpp
)

target_include_directories(${Albatross}.Config.LoadServerConfig PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Config.LoadServerConfig
        PUBLIC
        ${Albatross}.Config.ServerConfig
        ${Albatross}.Types
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
        JsonCpp::JsonCpp
)
##############################################################################
