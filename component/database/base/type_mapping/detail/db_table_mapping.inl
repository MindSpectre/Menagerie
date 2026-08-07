#pragma once

#include <menagerie/beavers>

#include <db_table.hpp>

namespace menagerie::db {

    /**
     * @brief Infers the SQL type from this table's stored provider and adds the field.
     * @throw std::logic_error if no provider was set; use the
     *        DynamicTable(name, provider) constructor, or the two-argument
     *        add_field(name, db_type) overload, instead.
     */
    template <typename T, beavers::IsStringLike StringTp>
    DynamicTable& DynamicTable::add_field(StringTp&& name) {
        if (provider_ == Providers::None) {
            throw std::logic_error{"Cannot infer SQL type: DynamicTable has no provider set. "
                                   "Use DynamicTable(name, provider) constructor."};
        }
        return add_field<T>(std::forward<StringTp>(name), sql_type<T>(provider_));
    }

}  // namespace menagerie::db
