##############################################################################
# SQL Type Mapping (C++ type -> SQL type string)
##############################################################################
add_library(${REEF}.Base.TypeMapping INTERFACE
        type_mapping/sql_type_mapping.hpp
        type_mapping/detail/db_table_mapping.inl
)
target_include_directories(${REEF}.Base.TypeMapping INTERFACE
        type_mapping/
)
target_link_libraries(${REEF}.Base.TypeMapping INTERFACE
        ${REEF}.Features
        ${REEF}.Base.Dialect
)
##############################################################################


##############################################################################
# Sql Params base class
##############################################################################
add_library(${REEF}.Base.Params INTERFACE
        params/sql_params.hpp
)
target_include_directories(${REEF}.Base.Params INTERFACE
        params/
)
target_link_libraries(${REEF}.Base.Params INTERFACE
        ${REEF}.Primitives
)

##############################################################################


##############################################################################
# Dialect concepts and CRTP base
##############################################################################
add_library(${REEF}.Base.Dialect INTERFACE
        dialect/dialect_concepts.hpp
)
target_include_directories(${REEF}.Base.Dialect INTERFACE
        dialect/
)
target_link_libraries(${REEF}.Base.Dialect INTERFACE
        ${REEF}.Base.Params
        ${REEF}.Features
)
##############################################################################

##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${REEF}.Base.Errors STATIC
        errors/db_exceptions.hpp
        errors/db_error_codes.hpp
)
target_include_directories(${REEF}.Base.Errors PUBLIC
        errors/
)
target_link_libraries(${REEF}.Base.Errors PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




