##############################################################################
#
##############################################################################
add_library(${REEF_POSTGRESQL}.Capabilities.Notifier STATIC
        postgres_notifier.cpp
        postgres_notifier.hpp
)

target_include_directories(${REEF_POSTGRESQL}.Capabilities.Notifier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REEF_POSTGRESQL}.Capabilities.Notifier PUBLIC

)
##############################################################################