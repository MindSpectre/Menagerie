##############################################################################
# PostgreSql Params
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Params STATIC
        postgres_params.hpp
        postgres_params.cpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Params PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.Params
        PUBLIC
        ${SAVANNA}.Base.Params
        ${SAVANNA_ELEPHANT}.Registries
)
##############################################################################