##############################################################################
# Features Target
##############################################################################
add_library(${REEF}.Features INTERFACE
        providers.hpp
)
target_include_directories(${REEF}.Features INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Provider compile definitions (CMake BUILD_X -> C++ X_ENABLED)
if (BUILD_POSTGRESQL)
    target_compile_definitions(${REEF}.Features INTERFACE POSTGRESQL_ENABLED)
endif ()
##############################################################################