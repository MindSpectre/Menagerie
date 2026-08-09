##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${Estuary}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${Estuary}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Server.AttachDefaultListeners
        PUBLIC
        ${Estuary}.Server.Core
        ${Estuary}.Config
        ${Estuary}.Drivers
)
##############################################################################
