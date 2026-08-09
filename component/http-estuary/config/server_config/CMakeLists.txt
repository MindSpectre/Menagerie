##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Estuary}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${Estuary}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Config.ServerConfig INTERFACE
        ${Estuary}.Config.ListenerConfig
        ${Estuary}.Config.Timeouts
        Menagerie::Common::Pangolin
)
##############################################################################
