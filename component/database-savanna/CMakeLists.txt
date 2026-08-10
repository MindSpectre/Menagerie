set(SAVANNA ${COMPONENT}.Savanna)

add_subdirectory(features)

add_subdirectory(base)

add_subdirectory(query)

add_subdirectory(primitives)

add_subdirectory(providers)

##############################################################################
# Savanna exported library -- the component umbrella over every built provider.
# Each provider target already carries the provider-agnostic core (features,
# base, primitives, query) transitively, so only the providers are listed here.
##############################################################################
if (BUILD_POSTGRESQL)
    list(APPEND SAVANNA_PROVIDER_LIBRARIES Menagerie::Component::Savanna::Elephant)
endif ()

add_combined_library(${SAVANNA}
        DIRECTORIES
        export/
        SOURCES
        export/menagerie/savanna
        LIBRARIES
        ${SAVANNA_PROVIDER_LIBRARIES}
)

add_library(Menagerie::Component::Savanna ALIAS ${SAVANNA})
##############################################################################
