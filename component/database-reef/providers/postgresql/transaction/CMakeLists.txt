##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${REEF_POSTGRESQL}.Transaction STATIC
        status/transaction_status.hpp
        options/transaction_options.hpp
        base/transaction.hpp
        base/transaction.cpp
        auto_transaction/auto_transaction.hpp
        auto_transaction/auto_transaction.cpp
        savepoint/savepoint.hpp
        savepoint/savepoint.cpp
        postgres_transaction.hpp
)
target_include_directories(${REEF_POSTGRESQL}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${REEF_POSTGRESQL}.Transaction
        PUBLIC
        ${REEF_POSTGRESQL}.Capabilities
        ${REEF_POSTGRESQL}.Errors
)
##############################################################################
