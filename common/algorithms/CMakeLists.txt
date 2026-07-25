set(ALGORITHMS ${COMMON}.Algorithms)

##############################################################################
# Sorter based on sliding window approach
##############################################################################
add_library(${ALGORITHMS}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/sliding_window_sorter.hpp
)
target_include_directories(${ALGORITHMS}.SlidingWindowSorter INTERFACE
        sliding_window_sorter/
)
##############################################################################

##############################################################################
# Algorithms exported library
##############################################################################
add_combined_library(${ALGORITHMS}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/algorithms
        LIBRARIES
        ${ALGORITHMS}.SlidingWindowSorter
)

add_library(Menagerie::Common::Algorithms ALIAS ${ALGORITHMS})
##############################################################################
