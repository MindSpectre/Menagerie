##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${DATABASE_POSTGRESQL}.Transaction STATIC
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
target_include_directories(${DATABASE_POSTGRESQL}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${DATABASE_POSTGRESQL}.Transaction
        PUBLIC
        ${DATABASE_POSTGRESQL}.Capabilities
        ${DATABASE_POSTGRESQL}.Errors
)
##############################################################################
