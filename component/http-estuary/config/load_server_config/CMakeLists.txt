##############################################################################
# Http Config — load_server_config / dump_server_config + config error types
##############################################################################
add_library(${Estuary}.Config.LoadServerConfig STATIC
        load_server_config.cpp
        load_server_config.hpp
)

target_include_directories(${Estuary}.Config.LoadServerConfig PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Config.LoadServerConfig
        PUBLIC
        ${Estuary}.Config.ServerConfig
        ${Estuary}.Types
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
        JsonCpp::JsonCpp
)
##############################################################################
