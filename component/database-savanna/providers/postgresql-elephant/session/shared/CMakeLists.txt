##############################################################################
# PostgreSQL Session Shared Types (PoolConfig)
# Consumed by both free_lock/ and blocking/ session variants.
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Session.Shared STATIC
        pool_config.hpp
        pool_config.cpp
)
target_include_directories(${SAVANNA_ELEPHANT}.Session.Shared PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.Session.Shared
        PUBLIC
        ${SAVANNA_ELEPHANT}.Config
        Menagerie::Common::Beaver
        Menagerie::Common::Pangolin
)
##############################################################################
