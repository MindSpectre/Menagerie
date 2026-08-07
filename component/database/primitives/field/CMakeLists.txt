##############################################################################
# Field Primitive Class
##############################################################################
add_library(${DATABASE}.Primitives.Field INTERFACE
        db_field_value.hpp
        db_field.hpp
        db_constraints.hpp
        schema/db_dynamic_field_schema.hpp
        schema/db_static_field_schema.hpp
        detail/db_field.inl
)

target_include_directories(${DATABASE}.Primitives.Field INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${DATABASE}.Primitives.Field INTERFACE
        Boost::container
        Menagerie::Common::Beavers
)
##############################################################################