set(MATH ${COMMON}.Math)

##############################################################################
# Math utilities
##############################################################################
add_library(${MATH}.Random STATIC
        random/random.cpp
        random/generators/number_generator.hpp
        random/generators/time_generator.hpp
        random/utils/random_utils.hpp
        random/base_random_generator.hpp
)

target_include_directories(${MATH}.Random PUBLIC
        random/utils
        random/
)
##############################################################################

##############################################################################
# Math exported library
##############################################################################
add_combined_library(${MATH}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/math
        LIBRARIES
        ${MATH}.Random
)

add_library(Menagerie::Common::Math ALIAS ${MATH})
##############################################################################
