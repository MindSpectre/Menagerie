##############################################################################
# Http Server — attach_default_listeners (config-driven listener wiring)
##############################################################################
add_library(${Albatross}.Server.AttachDefaultListeners STATIC
        attach_default_listeners.cpp
        attach_default_listeners.hpp
)

target_include_directories(${Albatross}.Server.AttachDefaultListeners PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Albatross}.Server.AttachDefaultListeners
        PUBLIC
        ${Albatross}.Server.Core
        ${Albatross}.Config
        ${Albatross}.Drivers
)
##############################################################################
