set(SAVANNA_ELEPHANT ${SAVANNA_PROVIDERS}.Elephant)

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

add_combined_library(${SAVANNA_ELEPHANT}
        DIRECTORIES
        export/
        SOURCES
        export/menagerie/postgresql
        LIBRARIES
        ${SAVANNA_ELEPHANT}.Config
        ${SAVANNA_ELEPHANT}.Dialect
        ${SAVANNA_ELEPHANT}.Params
        ${SAVANNA_ELEPHANT}.Result
        ${SAVANNA_ELEPHANT}.Capabilities
        ${SAVANNA_ELEPHANT}.TypeMapping
        ${SAVANNA_ELEPHANT}.Session
        ${SAVANNA_ELEPHANT}.Transaction
)

add_library(Menagerie::Component::Savanna::Elephant ALIAS ${SAVANNA_ELEPHANT})