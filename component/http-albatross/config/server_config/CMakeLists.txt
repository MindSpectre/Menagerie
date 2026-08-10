##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Albatross}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${Albatross}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Config.ServerConfig INTERFACE
        ${Albatross}.Config.ListenerConfig
        ${Albatross}.Config.Timeouts
        Menagerie::Common::Pangolin
)
##############################################################################
