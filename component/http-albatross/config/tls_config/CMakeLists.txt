##############################################################################
# Http Config — TlsConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Albatross}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${Albatross}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Config.TlsConfig INTERFACE
        Menagerie::Common::Pangolin
)
##############################################################################
