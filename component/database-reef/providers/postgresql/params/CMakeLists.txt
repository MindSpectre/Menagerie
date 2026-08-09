##############################################################################
# PostgreSql Params
##############################################################################
add_library(${REEF_POSTGRESQL}.Params STATIC
        postgres_params.hpp
        postgres_params.cpp
)
target_include_directories(${REEF_POSTGRESQL}.Params PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF_POSTGRESQL}.Params
        PUBLIC
        ${REEF}.Base.Params
        ${REEF_POSTGRESQL}.Registries
)
##############################################################################