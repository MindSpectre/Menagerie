set(DATABASE_POSTGRESQL ${DATABASE_PROVIDERS}.PostgreSQL)

add_subdirectory(registries)

add_subdirectory(dialect)

add_subdirectory(type_mapping)

add_subdirectory(params)

add_subdirectory(errors)

add_subdirectory(result)

add_subdirectory(config)

add_subdirectory(connection_holder)

add_subdirectory(capabilities)

add_subdirectory(session)

add_subdirectory(transaction)

add_combined_library(${DATABASE_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/menagerie/postgresql
        LIBRARIES
        ${DATABASE_POSTGRESQL}.Config
        ${DATABASE_POSTGRESQL}.Dialect
        ${DATABASE_POSTGRESQL}.Params
        ${DATABASE_POSTGRESQL}.Result
        ${DATABASE_POSTGRESQL}.Capabilities
        ${DATABASE_POSTGRESQL}.TypeMapping
        ${DATABASE_POSTGRESQL}.Session
        ${DATABASE_POSTGRESQL}.Transaction
)

add_library(Menagerie::Component::Database::PostgreSQL ALIAS ${DATABASE_POSTGRESQL})