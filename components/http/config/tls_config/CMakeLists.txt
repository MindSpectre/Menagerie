##############################################################################
# Http Config — TlsConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${HTTP}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${HTTP}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Config.TlsConfig INTERFACE
        Menagerie::Common::Serialization
)
##############################################################################
