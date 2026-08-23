set(BEAVER ${COMMON}.Beaver)

##############################################################################
# Custom string library
##############################################################################
add_library(${BEAVER}.Strings INTERFACE
        strings/strings.hpp
)
target_include_directories(${BEAVER}.Strings INTERFACE
        strings/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${BEAVER}.MetaProgramming INTERFACE
        meta_programming/class_traits.hpp
        meta_programming/templates.hpp
        meta_programming/concepts.hpp
)
target_include_directories(${BEAVER}.MetaProgramming INTERFACE
        meta_programming/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${BEAVER}.Utilities INTERFACE
        utilities/macros.hpp
        utilities/utils.hpp
        utilities/types.hpp
        utilities/hash.hpp
        utilities/convert.hpp
)
target_include_directories(${BEAVER}.Utilities INTERFACE
        utilities/
)
target_link_libraries(${BEAVER}.Utilities INTERFACE
        ${BEAVER}.MetaProgramming
)
##############################################################################

##############################################################################
# Gears exported library
##############################################################################
add_combined_library(${BEAVER}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/beaver
        LIBRARIES
        ${BEAVER}.MetaProgramming
        ${BEAVER}.Utilities
        ${BEAVER}.Strings
)

add_library(Menagerie::Common::Beaver ALIAS ${BEAVER})
##############################################################################
