##############################################################################
# PostgreSQL Session Shared Types (PoolConfig)
# Consumed by both free_lock/ and blocking/ session variants.
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Session.Shared STATIC
        pool_config.hpp
        pool_config.cpp
)
target_include_directories(${DATABASE_POSTGRESQL}.Session.Shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE_POSTGRESQL}.Session.Shared
        PUBLIC
        ${DATABASE_POSTGRESQL}.Config
        Menagerie::Common::Beavers
        Menagerie::Common::Serialization
)
##############################################################################
