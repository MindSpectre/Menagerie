##############################################################################
# Ink Table — value-type fluent builder with column-DSL and row-DSL
##############################################################################
add_library(${CHAMELEON}.Table INTERFACE
        table/table.hpp
)
target_include_directories(${CHAMELEON}.Table INTERFACE table)
target_link_libraries(${CHAMELEON}.Table INTERFACE
        ${CHAMELEON}.Base
)
##############################################################################

##############################################################################
# Ink Section — title + (label, value) pairs, multi-line value alignment
##############################################################################
add_library(${CHAMELEON}.Section INTERFACE
        section/section.hpp
)
target_include_directories(${CHAMELEON}.Section INTERFACE section)
target_link_libraries(${CHAMELEON}.Section INTERFACE
        ${CHAMELEON}.Base
)
##############################################################################

##############################################################################
# Ink Box — titled bordered block around body
##############################################################################
add_library(${CHAMELEON}.Box INTERFACE
        box/box.hpp
)
target_include_directories(${CHAMELEON}.Box INTERFACE box)
target_link_libraries(${CHAMELEON}.Box INTERFACE
        ${CHAMELEON}.Base
)
##############################################################################
