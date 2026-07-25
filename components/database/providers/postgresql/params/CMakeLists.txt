##############################################################################
# PostgreSql Params
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Params STATIC
        postgres_params.hpp
        postgres_params.cpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Params PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE_POSTGRESQL}.Params
        PUBLIC
        ${DATABASE}.Base.Params
        ${DATABASE_POSTGRESQL}.Registries
)
##############################################################################