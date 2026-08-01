##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${Http}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${Http}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Http}.Server.AttachDefaultListeners
        PUBLIC
        ${Http}.Server.Core
        ${Http}.Config
        ${Http}.Drivers
)
##############################################################################
