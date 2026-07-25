set(CHRONO ${COMMON}.Chrono)

##############################################################################
# Utilities for chrono tools
##############################################################################
add_library(${CHRONO}.Utilities INTERFACE
        utilities/chrono_utils.hpp
)
target_include_directories(${CHRONO}.Utilities INTERFACE
        utilities/
)
target_link_libraries(${CHRONO}.Utilities INTERFACE
        Menagerie::Common::Beavers
        Boost::asio
)
##############################################################################

##############################################################################
# Stopwatch class with measure purposes # TODO: Refactor
##############################################################################
add_library(${CHRONO}.Stopwatch INTERFACE
        stopwatch/stopwatch.hpp
        stopwatch/printing_stopwatch.hpp
)
target_include_directories(${CHRONO}.Stopwatch INTERFACE
        stopwatch/
)
target_link_libraries(${CHRONO}.Stopwatch INTERFACE
        Menagerie::Common::Beavers
)
##############################################################################

##############################################################################
# Clock for checking time
##############################################################################
add_library(${CHRONO}.Clock INTERFACE
        clock/clock.hpp
        clock/tsc_clock.hpp
        clock/imf_date.hpp
)
target_include_directories(${CHRONO}.Clock INTERFACE
        clock/
)
##############################################################################

##############################################################################
# Timer for managing function flow
##############################################################################
add_library(${CHRONO}.Timer STATIC
        timer/detail/timer.inl
        timer/detail/cancellation_token.hpp
        timer/timer.hpp
        timer/timer.cpp
)
target_include_directories(${CHRONO}.Timer PUBLIC
        timer/
)
target_link_libraries(${CHRONO}.Timer PUBLIC
        Menagerie::Common::Beavers
        Menagerie::Common::Multithread
)
##############################################################################

##############################################################################
# Chrono exported library
##############################################################################
add_combined_library(${CHRONO}
        SOURCES
        export/menagerie/chrono
        DIRECTORIES
        export
        LIBRARIES
        ${CHRONO}.Clock
        ${CHRONO}.Stopwatch
        ${CHRONO}.Utilities
        ${CHRONO}.Timer
)

add_library(Menagerie::Common::Chrono ALIAS ${CHRONO})
##############################################################################
