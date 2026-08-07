##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Dialect STATIC
        postgres_dialect.hpp
        postgres_dialect.cpp
        detail/postgres_dialect.inl
)
target_include_directories(${DATABASE_POSTGRESQL}.Dialect PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE_POSTGRESQL}.Dialect
        PUBLIC
        ${DATABASE}.Base.Dialect
        ${DATABASE}.Base.TypeMapping
        ${DATABASE_POSTGRESQL}.Registries
        Menagerie::Common::Beavers
        PRIVATE
        ${DATABASE_POSTGRESQL}.Params
)
##############################################################################