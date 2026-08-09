##############################################################################
#
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Capabilities.Pipeline STATIC
        postgres_pipeline.cpp
        postgres_pipeline.hpp
)

target_include_directories(${SAVANNA_ELEPHANT}.Capabilities.Pipeline PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA_ELEPHANT}.Capabilities.Pipeline PUBLIC

)
##############################################################################