##############################################################################
#
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Capabilities.Notifier STATIC
        postgres_notifier.cpp
        postgres_notifier.hpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Notifier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Notifier PUBLIC

)
##############################################################################