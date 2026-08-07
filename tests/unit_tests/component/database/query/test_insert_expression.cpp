// Behaviour of InsertExpr::values(Record)/batch(Records) when the record's schema and the into(...)
// column list disagree. No dialect or connection is involved - the mismatch is detected while the
// expression is being built, before anything is compiled to SQL.
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <db_record.hpp>
#include <db_table.hpp>
#include <gtest/gtest.h>
#include <query_expressions.hpp>

using namespace menagerie::db;

class InsertExpressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        schema = std::make_shared<DynamicTable>("users");
        schema->add_field<std::string>("name", "TEXT");
        schema->add_field<int>("age", "INTEGER");
    }

    [[nodiscard]] Record make_record(const std::string& name, const int age) const {
        Record record{schema};
        record["name"].set(name);
        record["age"].set(age);
        return record;
    }

    std::shared_ptr<DynamicTable> schema;
};

// A missing column must reach the caller as an exception. Before this was fixed values()/batch() were
// noexcept, so the std::out_of_range thrown by Record::operator[] terminated the process instead.
TEST_F(InsertExpressionTest, ValuesFromRecordThrowsOnColumnMissingFromSchema) {
    const Record record = make_record("Bob", 35);

    EXPECT_THROW(std::ignore = insert_into(std::string{"users"}).into({"name", "email"}).values(record),
                 std::out_of_range);
}

TEST_F(InsertExpressionTest, BatchThrowsOnColumnMissingFromSchema) {
    const std::vector records = {make_record("A", 1), make_record("B", 2)};

    EXPECT_THROW(std::ignore = insert_into(std::string{"users"}).into({"name", "email"}).batch(records),
                 std::out_of_range);
}

// The moving overload takes a separate branch through the same lookup, so it needs its own case.
TEST_F(InsertExpressionTest, ValuesFromRvalueRecordThrowsOnColumnMissingFromSchema) {
    EXPECT_THROW(std::ignore =
                     insert_into(std::string{"users"}).into({"name", "email"}).values(make_record("Bob", 35)),
                 std::out_of_range);
}

// The point of the fix: the message has to be enough to diagnose the mismatch on its own, so it names
// the offending column, the schema the record actually carries, and the fields that schema defines.
TEST_F(InsertExpressionTest, MissingColumnMessageNamesColumnSchemaAndAvailableFields) {
    const Record record = make_record("Bob", 35);

    try {
        std::ignore = insert_into(std::string{"users"}).into({"name", "email"}).values(record);
        FAIL() << "expected std::out_of_range";
    } catch (const std::out_of_range& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("email"), std::string::npos) << message;
        EXPECT_NE(message.find("users"), std::string::npos) << message;
        EXPECT_NE(message.find("name"), std::string::npos) << message;
        EXPECT_NE(message.find("age"), std::string::npos) << message;
    }
}

// The reported row index counts rows already appended to this INSERT, so it points at the record that
// failed rather than at the start of the batch.
TEST_F(InsertExpressionTest, MissingColumnMessageReportsRowIndex) {
    const Record record = make_record("Bob", 35);

    try {
        std::ignore = insert_into(std::string{"users"})
                          .into({"name", "email"})
                          .values({FieldValue{std::string{"literal"}}, FieldValue{std::string{"a@b"}}})
                          .values(record);
        FAIL() << "expected std::out_of_range";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string{e.what()}.find("row 1"), std::string::npos) << e.what();
    }
}

TEST_F(InsertExpressionTest, MissingColumnMessageHandlesRecordWithNoFields) {
    const Record record{std::make_shared<DynamicTable>("empty_table")};

    try {
        std::ignore = insert_into(std::string{"users"}).into({"name"}).values(record);
        FAIL() << "expected std::out_of_range";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string{e.what()}.find("empty_table"), std::string::npos) << e.what();
    }
}

// Guard the distinction the fix rests on: the overloads that can only fail by allocating keep the
// noexcept they always had, the ones that can report a caller error lose it. declval keeps argument
// construction out of the noexcept operand.
TEST_F(InsertExpressionTest, NoexceptClassificationMatchesWhatEachOverloadCanFailWith) {
    using Insert = InsertExpr<std::string>;

    static_assert(!noexcept(std::declval<Insert&>().values(std::declval<const Record&>())));
    static_assert(!noexcept(std::declval<Insert&>().values(std::declval<Record&&>())));
    static_assert(!noexcept(std::declval<Insert&>().batch(std::declval<const std::vector<Record>&>())));

    static_assert(noexcept(std::declval<Insert&>().into(std::declval<std::initializer_list<std::string>>())));
    static_assert(noexcept(std::declval<Insert&>().values(std::declval<std::initializer_list<FieldValue>>())));
    SUCCEED();
}

// The rvalue overload takes the moving branch through Field::raw_value() &&, which hands out a
// reference to the field's variant rather than a copy of it; the row must still come out intact.
TEST_F(InsertExpressionTest, ValuesFromRvalueRecordMovesValuesIntoRow) {
    const auto expr = insert_into(std::string{"users"}).into({"name", "age"}).values(make_record("Bob", 35));

    ASSERT_EQ(expr.rows().size(), 1U);
    ASSERT_EQ(expr.rows()[0].size(), 2U);
    EXPECT_EQ(std::get<std::string>(expr.rows()[0][0]), "Bob");
    EXPECT_EQ(std::get<int>(expr.rows()[0][1]), 35);
}

// A matching record must still build the row it always did.
TEST_F(InsertExpressionTest, ValuesFromMatchingRecordAppendsRow) {
    const Record record = make_record("Bob", 35);

    const auto expr = insert_into(std::string{"users"}).into({"name", "age"}).values(record);

    ASSERT_EQ(expr.rows().size(), 1U);
    ASSERT_EQ(expr.rows()[0].size(), 2U);
    EXPECT_EQ(std::get<std::string>(expr.rows()[0][0]), "Bob");
    EXPECT_EQ(std::get<int>(expr.rows()[0][1]), 35);
}
