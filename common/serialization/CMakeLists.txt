set(SERIALIZATION ${COMMON}.Serialization)

##############################################################################
# Field descriptor and concepts (header-only)
##############################################################################
add_library(${SERIALIZATION}.Config INTERFACE
        config/field.hpp
        config/serial_concepts.hpp
        config/config_interface.hpp
)
target_include_directories(${SERIALIZATION}.Config INTERFACE
        config
)
target_link_libraries(${SERIALIZATION}.Config INTERFACE
        Menagerie::Common::Beavers
)
##############################################################################

##############################################################################
# JSON format support
##############################################################################
add_library(${SERIALIZATION}.Formats.Json STATIC
        formats/json/json.hpp
        formats/json/json.cpp
)
target_include_directories(${SERIALIZATION}.Formats.Json PUBLIC
        formats
)
target_link_libraries(${SERIALIZATION}.Formats.Json PUBLIC
        ${SERIALIZATION}.Config
        JsonCpp::JsonCpp
)
##############################################################################

##############################################################################
# Serialization exported library
##############################################################################
add_combined_library(${SERIALIZATION}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/serialization
        LIBRARIES
        ${SERIALIZATION}.Config
        ${SERIALIZATION}.Formats.Json
)

add_library(Menagerie::Common::Serialization ALIAS ${SERIALIZATION})
##############################################################################
