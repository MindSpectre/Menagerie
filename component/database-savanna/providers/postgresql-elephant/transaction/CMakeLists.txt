##############################################################################
# PostgreSql Transaction
##############################################################################
add_library(${SAVANNA_ELEPHANT}.Transaction STATIC
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
target_include_directories(${SAVANNA_ELEPHANT}.Transaction PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(${SAVANNA_ELEPHANT}.Transaction
        PUBLIC
        ${SAVANNA_ELEPHANT}.Capabilities
        ${SAVANNA_ELEPHANT}.Errors
)
##############################################################################
