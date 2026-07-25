set(BEAVERS ${COMMON}.Beavers)

##############################################################################
# Outcome class for convenient handling
##############################################################################
add_library(${BEAVERS}.Outcome INTERFACE
        outcome/outcome.hpp
)
target_include_directories(${BEAVERS}.Outcome INTERFACE
        outcome/
)
target_link_libraries(${BEAVERS}.Outcome INTERFACE
        ${BEAVERS}.MetaProgramming
)
##############################################################################

##############################################################################
# Custom string library
##############################################################################
add_library(${BEAVERS}.Strings INTERFACE
        strings/strings.hpp
)
target_include_directories(${BEAVERS}.Strings INTERFACE
        strings/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${BEAVERS}.MetaProgramming INTERFACE
        meta_programming/class_traits.hpp
        meta_programming/templates.hpp
        meta_programming/concepts.hpp
)
target_include_directories(${BEAVERS}.MetaProgramming INTERFACE
        meta_programming/
)
##############################################################################

##############################################################################
# Traits for classes
##############################################################################
add_library(${BEAVERS}.Utilities INTERFACE
        utilities/macros.hpp
        utilities/utils.hpp
        utilities/types.hpp
        utilities/hash.hpp
        utilities/convert.hpp
)
target_include_directories(${BEAVERS}.Utilities INTERFACE
        utilities/
)
target_link_libraries(${BEAVERS}.Utilities INTERFACE
        ${BEAVERS}.MetaProgramming
)
##############################################################################

##############################################################################
# Gears exported library
##############################################################################
add_combined_library(${BEAVERS}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/beavers
        LIBRARIES
        ${BEAVERS}.Outcome
        ${BEAVERS}.MetaProgramming
        ${BEAVERS}.Utilities
        ${BEAVERS}.Strings
)

add_library(Menagerie::Common::Beavers ALIAS ${BEAVERS})
##############################################################################
