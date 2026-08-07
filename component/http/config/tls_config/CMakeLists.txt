##############################################################################
# Http Config — TlsConfig (ConfigInterface; JSON-loadable)
##############################################################################
add_library(${Http}.Config.TlsConfig INTERFACE tls_config.hpp)

target_include_directories(${Http}.Config.TlsConfig INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Config.TlsConfig INTERFACE
        Menagerie::Common::Serialization
)
##############################################################################
