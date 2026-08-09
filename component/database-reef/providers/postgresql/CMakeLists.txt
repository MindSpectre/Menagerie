set(REEF_POSTGRESQL ${REEF_PROVIDERS}.PostgreSQL)

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

add_combined_library(${REEF_POSTGRESQL}
        DIRECTORIES
        export/
        SOURCES
        export/menagerie/postgresql
        LIBRARIES
        ${REEF_POSTGRESQL}.Config
        ${REEF_POSTGRESQL}.Dialect
        ${REEF_POSTGRESQL}.Params
        ${REEF_POSTGRESQL}.Result
        ${REEF_POSTGRESQL}.Capabilities
        ${REEF_POSTGRESQL}.TypeMapping
        ${REEF_POSTGRESQL}.Session
        ${REEF_POSTGRESQL}.Transaction
)

add_library(Menagerie::Component::Reef::PostgreSQL ALIAS ${REEF_POSTGRESQL})