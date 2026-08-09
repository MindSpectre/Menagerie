##############################################################################
# PostgreSql Type Mapping
##############################################################################
add_library(${REEF_POSTGRESQL}.TypeMapping STATIC
        postgres_type_mapping.hpp
)
target_include_directories(${REEF_POSTGRESQL}.TypeMapping PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF_POSTGRESQL}.TypeMapping
        PUBLIC
        ${REEF}.Base.TypeMapping
        ${REEF_POSTGRESQL}.Registries
)
##############################################i################################