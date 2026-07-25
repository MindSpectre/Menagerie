set(DATABASE_PROVIDERS ${DATABASE}.Providers)

if (BUILD_POSTGRESQL)
    message("PostgreSQL Client will be built")
    add_subdirectory(postgresql)
endif ()