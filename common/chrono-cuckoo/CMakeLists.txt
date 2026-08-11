set(CUCKOO ${COMMON}.Cuckoo)

##############################################################################
# Utilities for chrono tools
##############################################################################
add_library(${CUCKOO}.Utilities INTERFACE
        utilities/chrono_utils.hpp
)
target_include_directories(${CUCKOO}.Utilities INTERFACE
        utilities/
)
target_link_libraries(${CUCKOO}.Utilities INTERFACE
        Menagerie::Common::Beaver
        Boost::asio
)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes # TODO: Refactor
##############################################################################
add_library(${CUCKOO}.Stopwatch INTERFACE
        stopwatch/stopwatch.hpp
        stopwatch/printing_stopwatch.hpp
)
target_include_directories(${CUCKOO}.Stopwatch INTERFACE
        stopwatch/
)
target_link_libraries(${CUCKOO}.Stopwatch INTERFACE
        Menagerie::Common::Beaver
)
##############################################################################

##############################################################################
# Clock for checking time
##############################################################################
add_library(${CUCKOO}.Clock INTERFACE
        clock/clock.hpp
        clock/tsc_clock.hpp
        clock/imf_date.hpp
)
target_include_directories(${CUCKOO}.Clock INTERFACE
        clock/
)
##############################################################################

##############################################################################
# Cuckoo exported library
##############################################################################
add_combined_library(${CUCKOO}
        SOURCES
        export/menagerie/cuckoo
        DIRECTORIES
        export
        LIBRARIES
        ${CUCKOO}.Clock
        ${CUCKOO}.Stopwatch
        ${CUCKOO}.Utilities
)

add_library(Menagerie::Common::Cuckoo ALIAS ${CUCKOO})
##############################################################################
