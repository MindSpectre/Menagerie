##############################################################################
#
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Capabilities.Notifier STATIC
        postgres_notifier.cpp
        postgres_notifier.hpp
)

target_include_directories(${DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DATABASE_POSTGRESQL}.Capabilities.Notifier PUBLIC

)
##############################################################################