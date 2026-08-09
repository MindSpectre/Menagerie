#pragma once

#include <memory>
#include <string_view>

#include <db_constraints.hpp>
#include <db_static_table.hpp>
#include <db_table.hpp>

namespace menagerie::test {
    using namespace reef::constraints;

    // Static table type aliases
    using UsersTable = reef::StaticTable<"users",
                                       reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       reef::StaticFieldSchema<std::string, "name">,
                                       reef::StaticFieldSchema<int, "age">,
                                       reef::StaticFieldSchema<bool, "active">>;

    using UsersExtendedTable = reef::StaticTable<"users",
                                               reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                               reef::StaticFieldSchema<std::string, "name">,
                                               reef::StaticFieldSchema<int, "age">,
                                               reef::StaticFieldSchema<bool, "active">,
                                               reef::StaticFieldSchema<std::string, "department">,
                                               reef::StaticFieldSchema<double, "salary">>;

    using PostsTable = reef::StaticTable<"posts",
                                       reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       reef::StaticFieldSchema<int, "user_id">,
                                       reef::StaticFieldSchema<std::string, "title">,
                                       reef::StaticFieldSchema<bool, "published">>;

    using OrdersTable = reef::StaticTable<"orders",
                                        reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                        reef::StaticFieldSchema<int, "user_id">,
                                        reef::StaticFieldSchema<double, "amount">,
                                        reef::StaticFieldSchema<bool, "completed">>;

    using OrdersExtendedTable = reef::StaticTable<"orders",
                                                reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                                reef::StaticFieldSchema<int, "user_id">,
                                                reef::StaticFieldSchema<double, "amount">,
                                                reef::StaticFieldSchema<bool, "completed">,
                                                reef::StaticFieldSchema<std::string, "status">,
                                                reef::StaticFieldSchema<std::string, "created_date">>;

    using CommentsTable = reef::StaticTable<"comments",
                                          reef::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                          reef::StaticFieldSchema<int, "post_id">,
                                          reef::StaticFieldSchema<int, "user_id">,
                                          reef::StaticFieldSchema<std::string, "content">>;

    // DDL strings for schema setup
    struct SchemaDDL {
        static std::string_view users_table(reef::Providers dialect);
        static std::string_view users_extended_table(reef::Providers dialect);
        static std::string_view posts_table(reef::Providers dialect);
        static std::string_view orders_table(reef::Providers dialect);
        static std::string_view orders_extended_table(reef::Providers dialect);
        static std::string_view comments_table(reef::Providers dialect);
        static std::string_view drop_all(reef::Providers dialect);
    };

    struct TestSchemas {
        UsersTable users;
        UsersExtendedTable users_extended;
        PostsTable posts;
        OrdersTable orders;
        OrdersExtendedTable orders_extended;
        CommentsTable comments;

        // DynamicTable for Record-based tests (DDL, insert with records, etc.)
        std::shared_ptr<reef::DynamicTable> users_dynamic;

        static TestSchemas create(reef::Providers provider);
    };

}  // namespace menagerie::test
