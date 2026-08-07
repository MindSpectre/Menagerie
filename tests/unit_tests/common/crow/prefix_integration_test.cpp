#include <algorithm>
#include <memory>
#include <menagerie/crow>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <detail/prefix_filter.hpp>
#include <gtest/gtest.h>

namespace {

    // Capture sink — records (prefix, message) pairs for everything that
    // passes should_log. Threshold is ignored (caller controls via filter).
    class CaptureSink final : public menagerie::crow::Sink {
    public:
        explicit CaptureSink(menagerie::crow::PrefixFilter filter = {})
            : filter_{std::move(filter)} {
        }

        void process(const menagerie::crow::LogEvent& event) noexcept override {
            if (!should_log(event.level, event.prefix.view())) {
                return;
            }
            std::lock_guard lock{mutex_};
            entries_.emplace_back(std::string{event.prefix.view()}, event.message);
        }

        void flush() noexcept override {
        }

        [[nodiscard]] bool should_log(menagerie::crow::LogLevel, std::string_view prefix) const noexcept override {
            return filter_.accepts(prefix);
        }

        [[nodiscard]] std::vector<std::pair<std::string, std::string>> entries() const {
            std::lock_guard lock{mutex_};
            return entries_;
        }

    private:
        menagerie::crow::PrefixFilter filter_;
        mutable std::mutex mutex_;
        std::vector<std::pair<std::string, std::string>> entries_;
    };

    bool contains(const std::vector<std::pair<std::string, std::string>>& es,
                  std::string_view prefix,
                  std::string_view message) {
        return std::ranges::any_of(es,
                                   [&](const auto& pair) { return pair.first == prefix && pair.second == message; });
    }

}  // namespace

TEST(PrefixIntegrationTest, TwoSinksSeeDifferentSubsetsOfSameStream) {
    auto allow_foo = std::make_shared<CaptureSink>(menagerie::crow::PrefixFilter::allow({"Foo"}));
    auto deny_bar  = std::make_shared<CaptureSink>(menagerie::crow::PrefixFilter::deny({"Bar"}));

    menagerie::crow::Logger logger;
    logger.add_sink(allow_foo);
    logger.add_sink(deny_bar);

    logger.log(menagerie::crow::LogLevel::Info, "Foo", "hi from foo");
    logger.log(menagerie::crow::LogLevel::Info, "Bar", "hi from bar");
    logger.log(menagerie::crow::LogLevel::Info, "Baz", "hi from baz");
    logger.log(menagerie::crow::LogLevel::Info, "", "hi from empty");

    logger.shutdown();

    const auto allow_entries = allow_foo->entries();
    const auto deny_entries  = deny_bar->entries();

    // allow_foo: only "Foo" and empty (default admits empty)
    EXPECT_TRUE(contains(allow_entries, "Foo", "hi from foo"));
    EXPECT_TRUE(contains(allow_entries, "", "hi from empty"));
    EXPECT_FALSE(contains(allow_entries, "Bar", "hi from bar"));
    EXPECT_FALSE(contains(allow_entries, "Baz", "hi from baz"));

    // deny_bar: everything except "Bar"
    EXPECT_TRUE(contains(deny_entries, "Foo", "hi from foo"));
    EXPECT_TRUE(contains(deny_entries, "Baz", "hi from baz"));
    EXPECT_TRUE(contains(deny_entries, "", "hi from empty"));
    EXPECT_FALSE(contains(deny_entries, "Bar", "hi from bar"));
}

TEST(PrefixIntegrationTest, BlockEmptyPrefixRejectsUnprefixedLogs) {
    auto sink = std::make_shared<CaptureSink>(menagerie::crow::PrefixFilter::allow({"Foo"}).block_empty_prefix());

    menagerie::crow::Logger logger;
    logger.add_sink(sink);

    logger.log(menagerie::crow::LogLevel::Info, "Foo", "yes");
    logger.log(menagerie::crow::LogLevel::Info, "", "no");

    logger.shutdown();

    const auto entries = sink->entries();
    EXPECT_TRUE(contains(entries, "Foo", "yes"));
    EXPECT_FALSE(contains(entries, "", "no"));
}
