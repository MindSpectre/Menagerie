##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${HTTP}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${HTTP}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Config.ServerConfig INTERFACE
        ${HTTP}.Config.ListenerConfig
        ${HTTP}.Config.Timeouts
        Menagerie::Common::Serialization
)
##############################################################################
