##############################################################################
# Http Config — TlsConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Estuary}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${Estuary}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Config.TlsConfig INTERFACE
        Menagerie::Common::Pangolin
)
##############################################################################
