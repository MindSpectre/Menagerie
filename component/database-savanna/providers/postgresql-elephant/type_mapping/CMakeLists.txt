##############################################################################
# PostgreSql Type Mapping
##############################################################################
add_library(${SAVANNA_ELEPHANT}.TypeMapping STATIC
        postgres_type_mapping.hpp
)
target_include_directories(${SAVANNA_ELEPHANT}.TypeMapping PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.TypeMapping
        PUBLIC
        ${SAVANNA}.Base.TypeMapping
        ${SAVANNA_ELEPHANT}.Registries
)
##############################################i################################