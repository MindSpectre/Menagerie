##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${HTTP}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${HTTP}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${HTTP}.Server.AttachDefaultListeners
        PUBLIC
        ${HTTP}.Server.Core
        ${HTTP}.Config
        ${HTTP}.Drivers
)
##############################################################################
