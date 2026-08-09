#pragma once

#include <memory>
#include <string_view>

#include <db_constraints.hpp>
#include <db_static_table.hpp>
#include <db_table.hpp>

namespace menagerie::test {
    using namespace savanna::constraints;

    // Static table type aliases
    using UsersTable = savanna::StaticTable<"users",
                                       savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       savanna::StaticFieldSchema<std::string, "name">,
                                       savanna::StaticFieldSchema<int, "age">,
                                       savanna::StaticFieldSchema<bool, "active">>;

    using UsersExtendedTable = savanna::StaticTable<"users",
                                               savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                               savanna::StaticFieldSchema<std::string, "name">,
                                               savanna::StaticFieldSchema<int, "age">,
                                               savanna::StaticFieldSchema<bool, "active">,
                                               savanna::StaticFieldSchema<std::string, "department">,
                                               savanna::StaticFieldSchema<double, "salary">>;

    using PostsTable = savanna::StaticTable<"posts",
                                       savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                       savanna::StaticFieldSchema<int, "user_id">,
                                       savanna::StaticFieldSchema<std::string, "title">,
                                       savanna::StaticFieldSchema<bool, "published">>;

    using OrdersTable = savanna::StaticTable<"orders",
                                        savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                        savanna::StaticFieldSchema<int, "user_id">,
                                        savanna::StaticFieldSchema<double, "amount">,
                                        savanna::StaticFieldSchema<bool, "completed">>;

    using OrdersExtendedTable = savanna::StaticTable<"orders",
                                                savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                                savanna::StaticFieldSchema<int, "user_id">,
                                                savanna::StaticFieldSchema<double, "amount">,
                                                savanna::StaticFieldSchema<bool, "completed">,
                                                savanna::StaticFieldSchema<std::string, "status">,
                                                savanna::StaticFieldSchema<std::string, "created_date">>;

    using CommentsTable = savanna::StaticTable<"comments",
                                          savanna::StaticFieldSchema<int, "id", PrimaryKey, NotNull>,
                                          savanna::StaticFieldSchema<int, "post_id">,
                                          savanna::StaticFieldSchema<int, "user_id">,
                                          savanna::StaticFieldSchema<std::string, "content">>;

    // DDL strings for schema setup
    struct SchemaDDL {
        static std::string_view users_table(savanna::Providers dialect);
        static std::string_view users_extended_table(savanna::Providers dialect);
        static std::string_view posts_table(savanna::Providers dialect);
        static std::string_view orders_table(savanna::Providers dialect);
        static std::string_view orders_extended_table(savanna::Providers dialect);
        static std::string_view comments_table(savanna::Providers dialect);
        static std::string_view drop_all(savanna::Providers dialect);
    };

    struct TestSchemas {
        UsersTable users;
        UsersExtendedTable users_extended;
        PostsTable posts;
        OrdersTable orders;
        OrdersExtendedTable orders_extended;
        CommentsTable comments;

        // DynamicTable for Record-based tests (DDL, insert with records, etc.)
        std::shared_ptr<savanna::DynamicTable> users_dynamic;

        static TestSchemas create(savanna::Providers provider);
    };

}  // namespace menagerie::test
