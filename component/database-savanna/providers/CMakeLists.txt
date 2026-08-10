set(SAVANNA_PROVIDERS ${SAVANNA}.Providers)

if (BUILD_POSTGRESQL)
    message("PostgreSQL Client will be built")
    add_subdirectory(postgresql-elephant)
endif ()