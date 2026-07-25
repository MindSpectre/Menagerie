##############################################################################
# Postgres result class + views
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Result STATIC
        postgres_result.hpp
        views/postgres_result_views.hpp
        views/postgres_result_views.cpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Result PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DATABASE_POSTGRESQL}.Result PUBLIC
        ${DATABASE_POSTGRESQL}.Registries
        ${DATABASE}.Base.Errors
        Menagerie::Common::Beavers
        PostgreSQL::PostgreSQL
        PRIVATE
        ${DATABASE}.Primitives.Field
)

##############################################################################
