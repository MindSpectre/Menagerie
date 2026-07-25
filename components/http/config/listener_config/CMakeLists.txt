##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs)
##############################################################################
add_library(${HTTP}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${HTTP}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Config.ListenerConfig INTERFACE
        ${HTTP}.Config.TlsConfig
        ${HTTP}.Types.Enums
        Menagerie::Common::Serialization
)
##############################################################################
