set(PANGOLIN ${COMMON}.Pangolin)

##############################################################################
# Field descriptor and concepts (header-only)
##############################################################################
add_library(${PANGOLIN}.Config INTERFACE
        config/field.hpp
        config/serial_concepts.hpp
        config/config_interface.hpp
)
target_include_directories(${PANGOLIN}.Config INTERFACE
        config
)
target_link_libraries(${PANGOLIN}.Config INTERFACE
        Menagerie::Common::Beaver
)
##############################################################################

##############################################################################
# JSON format support
##############################################################################
add_library(${PANGOLIN}.Formats.Json STATIC
        formats/json/json.hpp
        formats/json/json.cpp
)
target_include_directories(${PANGOLIN}.Formats.Json PUBLIC
        formats
)
target_link_libraries(${PANGOLIN}.Formats.Json PUBLIC
        ${PANGOLIN}.Config
        JsonCpp::JsonCpp
)
##############################################################################

##############################################################################
# Pangolin exported library
##############################################################################
add_combined_library(${PANGOLIN}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/pangolin
        LIBRARIES
        ${PANGOLIN}.Config
        ${PANGOLIN}.Formats.Json
)

add_library(Menagerie::Common::Pangolin ALIAS ${PANGOLIN})
##############################################################################
