##############################################################################
# PostgreSQL Session Shared Types (PoolConfig)
# Consumed by both free_lock/ and blocking/ session variants.
##############################################################################
add_library(${REEF_POSTGRESQL}.Session.Shared STATIC
        pool_config.hpp
        pool_config.cpp
)
target_include_directories(${REEF_POSTGRESQL}.Session.Shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF_POSTGRESQL}.Session.Shared
        PUBLIC
        ${REEF_POSTGRESQL}.Config
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
)
##############################################################################
