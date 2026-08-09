##############################################################################
# SQL Type Mapping (C++ type -> SQL type string)
##############################################################################
add_library(${SAVANNA}.Base.TypeMapping INTERFACE
        type_mapping/sql_type_mapping.hpp
        type_mapping/detail/db_table_mapping.inl
)
target_include_directories(${SAVANNA}.Base.TypeMapping INTERFACE
        type_mapping/
)
target_link_libraries(${SAVANNA}.Base.TypeMapping INTERFACE
        ${SAVANNA}.Features
        ${SAVANNA}.Base.Dialect
)
##############################################################################


##############################################################################
# Sql Params base class
##############################################################################
add_library(${SAVANNA}.Base.Params INTERFACE
        params/sql_params.hpp
)
target_include_directories(${SAVANNA}.Base.Params INTERFACE
        params/
)
target_link_libraries(${SAVANNA}.Base.Params INTERFACE
        ${SAVANNA}.Primitives
)

##############################################################################


##############################################################################
# Dialect concepts and CRTP base
##############################################################################
add_library(${SAVANNA}.Base.Dialect INTERFACE
        dialect/dialect_concepts.hpp
)
target_include_directories(${SAVANNA}.Base.Dialect INTERFACE
        dialect/
)
target_link_libraries(${SAVANNA}.Base.Dialect INTERFACE
        ${SAVANNA}.Base.Params
        ${SAVANNA}.Features
)
##############################################################################

##############################################################################
# Database exceptions base classes
##############################################################################
add_library(${SAVANNA}.Base.Errors STATIC
        errors/db_exceptions.hpp
        errors/db_error_codes.hpp
)
target_include_directories(${SAVANNA}.Base.Errors PUBLIC
        errors/
)
target_link_libraries(${SAVANNA}.Base.Errors PUBLIC
        Boost::exception
        Boost::system
        Boost::stacktrace_backtrace
)
##############################################################################




