##############################################################################
# PostgreSql Dialect
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Dialect STATIC
        postgres_dialect.hpp
        postgres_dialect.cpp
        detail/postgres_dialect.inl
)
target_include_directories(${SAVANNA_ELEPHANT}.Dialect PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.Dialect
        PUBLIC
        ${SAVANNA}.Base.Dialect
        ${SAVANNA}.Base.TypeMapping
        ${SAVANNA_ELEPHANT}.Registries
        Menagerie::Common::Beaver
        PRIVATE
        ${SAVANNA_ELEPHANT}.Params
)
##############################################################################