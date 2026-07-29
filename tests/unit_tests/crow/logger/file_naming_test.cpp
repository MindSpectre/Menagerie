#include <filesystem>
#include <fstream>
#include <menagerie/crow>

#include <gtest/gtest.h>

using namespace menagerie::crow;
namespace fs = std::filesystem;

namespace {
    /// Per-test scratch directory under the binary's CWD, removed on teardown.
    class FileNamingTest : public ::testing::Test {
    protected:
        fs::path dir;

        void SetUp() override {
            dir = fs::path{"file_naming_"} += ::testing::UnitTest::GetInstance()->current_test_info()->name();
            fs::remove_all(dir);
            fs::create_directories(dir);
        }

        void TearDown() override {
            fs::remove_all(dir);
        }

        void write_file(const fs::path& path, const std::size_t bytes) const {
            std::ofstream out{path, std::ios::binary | std::ios::trunc};
            out << std::string(bytes, 'x');
        }

        [[nodiscard]] FileSinkConfig
        config(const bool rotate, const bool add_time, const std::uint64_t max_size = 1024) const {
            return FileSinkConfig::Builder{}
                .file(dir / "app.log")
                .rotate_file(rotate)
                .add_time_to_filename(add_time)
                .max_file_size(max_size)
                .finalize();
        }
    };
}  // namespace

TEST_F(FileNamingTest, PlainModeUsesTheConfiguredNameVerbatim) {
    std::error_code ec;
    EXPECT_EQ(detail::initial_path(config(false, false), ec), dir / "app.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, TimestampModeInsertsTheTimeBeforeTheExtension) {
    std::error_code ec;
    const auto path = detail::initial_path(config(false, true), ec);

    ASSERT_FALSE(ec);
    EXPECT_EQ(path.parent_path(), dir);
    EXPECT_TRUE(path.stem().string().starts_with("app_"));
    EXPECT_EQ(path.extension(), ".log");
}

TEST_F(FileNamingTest, IndexedModeStartsAtTheBaseNameWhenNothingExists) {
    std::error_code ec;
    EXPECT_EQ(detail::initial_path(config(true, false), ec), dir / "app.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, IndexedModeResumesTheHighestIndexWhileItHasRoom) {
    write_file(dir / "app.log", 1024);
    write_file(dir / "app_1.log", 10);
    std::error_code ec;

    EXPECT_EQ(detail::initial_path(config(true, false, 1024), ec), dir / "app_1.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, IndexedModeOpensTheNextIndexWhenTheHighestIsFull) {
    write_file(dir / "app.log", 1024);
    write_file(dir / "app_1.log", 1024);
    std::error_code ec;

    EXPECT_EQ(detail::initial_path(config(true, false, 1024), ec), dir / "app_2.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, IndexedModeIgnoresADirectoryShapedLikeARotatedFile) {
    // A directory can share a rotated file's name (e.g. a stray "app_2.log/" left
    // behind by something unrelated). Matching candidates by extension alone would
    // pick it as the resume target -- opening a directory as a log file always fails,
    // and with ec left clear here, the sink would start Dead and every janitor retry
    // would just re-resolve the same directory again: unrecoverable, not degraded.
    write_file(dir / "app.log", 10);
    write_file(dir / "app_1.log", 10);
    fs::create_directories(dir / "app_2.log");  // obstacle: a directory, not a file
    std::error_code ec;

    // Without the is_regular_file() guard this would resolve to the app_2.log
    // directory; the real file at index 1 (with room under max_size) is correct.
    EXPECT_EQ(detail::initial_path(config(true, false, 1024), ec), dir / "app_1.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, IndexedRotationAdvancesTheIndex) {
    std::error_code ec;
    const auto cfg = config(true, false);

    EXPECT_EQ(detail::next_path(cfg, dir / "app.log", ec), dir / "app_1.log");
    EXPECT_EQ(detail::next_path(cfg, dir / "app_1.log", ec), dir / "app_2.log");
    EXPECT_EQ(detail::next_path(cfg, dir / "app_9.log", ec), dir / "app_10.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, TimestampRotationReturnsTheCurrentPathWithinTheSameSecond) {
    std::error_code ec;
    const auto cfg     = config(true, true);
    const auto current = detail::initial_path(cfg, ec);
    ASSERT_FALSE(ec);

    const auto next = detail::next_path(cfg, current, ec);
    EXPECT_FALSE(ec);

    // Comparing two wall-clock reads for equality races a real second boundary, so the
    // condition is derived from what was actually observed rather than assumed: a
    // freshly computed "this instant" timestamped path either still matches current
    // (no tick since current was resolved -- next_path must carry on writing it) or it
    // doesn't (a boundary landed between the reads -- next_path must still have
    // produced that genuinely new path, not silently reused the old one).
    const auto expected_now = detail::timestamped_path(cfg);
    if (expected_now == current) {
        EXPECT_EQ(next, current);
    } else {
        EXPECT_EQ(next, expected_now);
    }
}

TEST_F(FileNamingTest, NonRotatingModesNeverAdvance) {
    std::error_code ec;
    EXPECT_EQ(detail::next_path(config(false, false), dir / "app.log", ec), dir / "app.log");
    EXPECT_FALSE(ec);
}

TEST_F(FileNamingTest, IndexParsingIgnoresUnrelatedNames) {
    EXPECT_EQ(detail::index_of("app", "app.log"), std::optional<std::uint32_t>{0});
    EXPECT_EQ(detail::index_of("app", "app_7.log"), std::optional<std::uint32_t>{7});
    EXPECT_EQ(detail::index_of("app", "other_3.log"), std::nullopt);
    EXPECT_EQ(detail::index_of("app", "app_x.log"), std::nullopt);
    EXPECT_EQ(detail::index_of("app", "app_2026-07-29T10:00:00.log"), std::nullopt);
}

TEST_F(FileNamingTest, RotationWithoutTimestampNowValidates) {
    // The old validate() rejected this combination; indexed rotation depends on it.
    EXPECT_NO_THROW(std::ignore = config(true, false));
}
