set(EXAMPLES ${PROJECT_NAME}.Examples)

if (BUILD_HTTP)
    message("Examples for Http will be built")
    add_subdirectory(http)
endif ()
