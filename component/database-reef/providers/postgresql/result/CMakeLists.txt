##############################################################################
# Postgres result class + views
##############################################################################
add_library(${REEF_POSTGRESQL}.Result STATIC
        postgres_result.hpp
        views/postgres_result_views.hpp
        views/postgres_result_views.cpp
)
target_include_directories(${REEF_POSTGRESQL}.Result PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REEF_POSTGRESQL}.Result PUBLIC
        ${REEF_POSTGRESQL}.Registries
        ${REEF}.Base.Errors
        Menagerie::Common::Beaver
        PostgreSQL::PostgreSQL
        PRIVATE
        ${REEF}.Primitives.Field
)

##############################################################################
