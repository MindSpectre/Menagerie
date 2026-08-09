##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${REEF_POSTGRESQL}.Dialect STATIC
        postgres_dialect.hpp
        postgres_dialect.cpp
        detail/postgres_dialect.inl
)
target_include_directories(${REEF_POSTGRESQL}.Dialect PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF_POSTGRESQL}.Dialect
        PUBLIC
        ${REEF}.Base.Dialect
        ${REEF}.Base.TypeMapping
        ${REEF_POSTGRESQL}.Registries
        Menagerie::Common::Beaver
        PRIVATE
        ${REEF_POSTGRESQL}.Params
)
##############################################################################