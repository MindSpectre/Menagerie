##############################################################################
# PostgreSql Type Mapping
##############################################################################
add_library(${DATABASE_POSTGRESQL}.TypeMapping STATIC
        postgres_type_mapping.hpp
)
target_include_directories(${DATABASE_POSTGRESQL}.TypeMapping PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE_POSTGRESQL}.TypeMapping
        PUBLIC
        ${DATABASE}.Base.TypeMapping
        ${DATABASE_POSTGRESQL}.Registries
)
##############################################i################################