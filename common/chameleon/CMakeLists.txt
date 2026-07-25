set(CHAMELEON ${COMMON}.Chameleon)

##############################################################################
# Chameleon Base - colors, glyphs, separators
##############################################################################
add_library(${CHAMELEON}.Base INTERFACE
        base/layout.hpp
        base/colors.hpp
        base/glyphs.hpp
)
target_include_directories(${CHAMELEON}.Base INTERFACE base)
target_link_libraries(${CHAMELEON}.Base INTERFACE
        Menagerie::Common::Beavers
)
##############################################################################

add_subdirectory(rendering)

##############################################################################
# Chameleon exported library
##############################################################################
add_combined_library(${CHAMELEON}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/chameleon
        LIBRARIES
        ${CHAMELEON}.Base
        ${CHAMELEON}.Table
        ${CHAMELEON}.Section
        ${CHAMELEON}.Box
)

add_library(Menagerie::Common::Chameleon ALIAS ${CHAMELEON})
##############################################################################
