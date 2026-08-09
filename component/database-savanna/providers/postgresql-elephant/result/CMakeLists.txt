##############################################################################
# Postgres result class + views
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Result STATIC
        postgres_result.hpp
        views/postgres_result_views.hpp
        views/postgres_result_views.cpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Result PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SAVANNA_ELEPHANT}.Result PUBLIC
        ${SAVANNA_ELEPHANT}.Registries
        ${SAVANNA}.Base.Errors
        Menagerie::Common::Beaver
        PostgreSQL::PostgreSQL
        PRIVATE
        ${SAVANNA}.Primitives.Field
)

##############################################################################
