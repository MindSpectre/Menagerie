##############################################################################
# Features Target
##############################################################################
add_library(${SAVANNA}.Features INTERFACE
        providers.hpp
)
target_include_directories(${SAVANNA}.Features INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Provider compile definitions (CMake BUILD_X -> C++ X_ENABLED)
if (BUILD_POSTGRESQL)
    target_compile_definitions(${SAVANNA}.Features INTERFACE POSTGRESQL_ENABLED)
endif ()
##############################################################################