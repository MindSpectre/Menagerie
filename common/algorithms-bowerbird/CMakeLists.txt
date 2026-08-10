set(BOWERBIRD ${COMMON}.Bowerbird)

##############################################################################
# Sorter based on sliding window approach
##############################################################################
add_library(${BOWERBIRD}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/sliding_window_sorter.hpp
)
target_include_directories(${BOWERBIRD}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/
)
##############################################################################

##############################################################################
# Bowerbird exported library
##############################################################################
add_combined_library(${BOWERBIRD}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/bowerbird
        LIBRARIES
        ${BOWERBIRD}.SlidingWindowSorter
)

add_library(Menagerie::Common::Bowerbird ALIAS ${BOWERBIRD})
##############################################################################
