set(RABBIT ${COMMON}.Rabbit)

##############################################################################
# Rabbit utilities
##############################################################################
add_library(${RABBIT}.Random STATIC
        random/random.cpp
        random/generators/number_generator.hpp
        random/generators/time_generator.hpp
        random/utils/random_utils.hpp
        random/base_random_generator.hpp
)

target_include_directories(${RABBIT}.Random PUBLIC
        random/utils
        random/
)
##############################################################################

##############################################################################
# Rabbit exported library
##############################################################################
add_combined_library(${RABBIT}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/rabbit
        LIBRARIES
        ${RABBIT}.Random
)

add_library(Menagerie::Common::Rabbit ALIAS ${RABBIT})
##############################################################################
