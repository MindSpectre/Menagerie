##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs)
##############################################################################
add_library(${Estuary}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${Estuary}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Config.ListenerConfig INTERFACE
        ${Estuary}.Config.TlsConfig
        ${Estuary}.Types.Enums
        Menagerie::Common::Pangolin
)
##############################################################################
