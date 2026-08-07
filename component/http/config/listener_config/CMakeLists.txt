##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs)
##############################################################################
add_library(${Http}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${Http}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Config.ListenerConfig INTERFACE
        ${Http}.Config.TlsConfig
        ${Http}.Types.Enums
        Menagerie::Common::Serialization
)
##############################################################################
