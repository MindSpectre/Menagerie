##############################################################################
# SQL Type Mapping (C++ type -> SQL type string)
##############################################################################
add_library(${DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/sql_type_mapping.hpp
        type_mapping/detail/db_table_mapping.inl
)
target_include_directories(${DATABASE}.Base.TypeMapping INTERFACE
        type_mapping/
)
target_link_libraries(${DATABASE}.Base.TypeMapping INTERFACE
        ${DATABASE}.Features
        ${DATABASE}.Base.Dialect
)
##############################################################################


##############################################################################
# Sql Params base class
##############################################################################
add_library(${DATABASE}.Base.Params INTERFACE
        params/sql_params.hpp
)
target_include_directories(${DATABASE}.Base.Params INTERFACE
        params/
)
target_link_libraries(${DATABASE}.Base.Params INTERFACE
        ${DATABASE}.Primitives
)

##############################################################################


##############################################################################
# Dialect concepts and CRTP base
##############################################################################
add_library(${DATABASE}.Base.Dialect INTERFACE
        dialect/dialect_concepts.hpp
)
target_include_directories(${DATABASE}.Base.Dialect INTERFACE
        dialect/
)
target_link_libraries(${DATABASE}.Base.Dialect INTERFACE
        ${DATABASE}.Base.Params
        ${DATABASE}.Features
)
##############################################################################

##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${DATABASE}.Base.Errors STATIC
        errors/db_exceptions.hpp
        errors/db_error_codes.hpp
)
target_include_directories(${DATABASE}.Base.Errors PUBLIC
        errors/
)
target_link_libraries(${DATABASE}.Base.Errors PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




