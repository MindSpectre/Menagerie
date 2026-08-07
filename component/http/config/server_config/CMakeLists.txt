##############################################################################
# Http Config — ServerConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Http}.Config.ServerConfig INTERFACE server_config.hpp)

target_include_directories(${Http}.Config.ServerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Config.ServerConfig INTERFACE
        ${Http}.Config.ListenerConfig
        ${Http}.Config.Timeouts
        Menagerie::Common::Serialization
)
##############################################################################
