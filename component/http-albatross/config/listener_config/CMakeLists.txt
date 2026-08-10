##############################################################################
# Http Config — ListenerConfig (+ Protocol/Transport string codecs)
##############################################################################
add_library(${Albatross}.Config.ListenerConfig INTERFACE listener_config.hpp)

target_include_directories(${Albatross}.Config.ListenerConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Config.ListenerConfig INTERFACE
        ${Albatross}.Config.TlsConfig
        ${Albatross}.Types.Enums
        Menagerie::Common::Pangolin
)
##############################################################################
