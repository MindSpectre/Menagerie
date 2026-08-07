#include <chrono>
#include <filesystem>
#include <fstream>
#include <menagerie/crow>
#include <set>
#include <string>

#include <gtest/gtest.h>

using namespace menagerie::crow;
namespace fs = std::filesystem;

namespace {
    class RotationTest : public ::testing::Test {
    protected:
        fs::path dir;

        void SetUp() override {
            dir = fs::path{"rotation_"} += ::testing::UnitTest::GetInstance()->current_test_info()->name();
            fs::remove_all(dir);
            fs::create_directories(dir);
        }

        void TearDown() override {
            fs::remove_all(dir);
        }

        [[nodiscard]] FileSinkConfig config(const bool rotate, const bool add_time,
                                            const std::uint64_t max_size) const {
            return FileSinkConfig::Builder{}
                .threshold(DBG)
                .file(dir / "app.log")
                .rotate_file(rotate)
                .add_time_to_filename(add_time)
                .max_file_size(max_size)
                .flush_each_entry(true)
                .finalize();
        }

        static LogEvent event_of(const std::string& message) {
            LogEvent event;
            event.level   = INF;
            event.message = message;
            return event;
        }

        [[nodiscard]] std::size_t files_in_dir() const {
            return static_cast<std::size_t>(std::distance(fs::directory_iterator{dir}, fs::directory_iterator{}));
        }

        /// The timestamp segment of a rotated name ("app_<timestamp>.log" -> "<timestamp>"),
        /// or the whole stem if candidate doesn't carry the "app_" prefix.
        [[nodiscard]] static std::string embedded_timestamp(const fs::path& candidate) {
            static constexpr std::string_view prefix = "app_";
            std::string stem                         = candidate.stem().string();
            if (stem.starts_with(prefix)) {
                stem.erase(0, prefix.size());
            }
            return stem;
        }
    };
}  // namespace

TEST_F(RotationTest, PlainModeKeepsOneFileAndNeverRotates) {
    FileSink<LightEntry> sink{config(false, false, 64)};

    for (int i = 0; i < 200; ++i) {
        sink.process(event_of("a message long enough to pass sixty four bytes quickly"));
    }
    sink.flush();

    EXPECT_EQ(files_in_dir(), 1U);
    EXPECT_EQ(sink.file_path(), dir / "app.log");
    EXPECT_GT(fs::file_size(dir / "app.log"), 64U);
}

TEST_F(RotationTest, IndexedModeAdvancesThroughIndices) {
    FileSink<LightEntry> sink{config(true, false, 128)};

    for (int i = 0; i < 200; ++i) {
        sink.process(event_of("a message long enough to cross the rotation threshold"));
    }
    sink.flush();

    EXPECT_TRUE(fs::exists(dir / "app.log"));
    EXPECT_TRUE(fs::exists(dir / "app_1.log"));
    EXPECT_TRUE(fs::exists(dir / "app_2.log"));
    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
}

TEST_F(RotationTest, TimestampedRotationKeepsWritingWithinTheSameSecond) {
    FileSink<LightEntry> sink{config(true, true, 64)};

    // Many rotations' worth of bytes, written far faster than the one-second filename
    // granularity, must not fan out into a file per rotation attempt within the same
    // second. A file-count assertion that assumes the burst fits in one second flakes
    // on a loaded box; a same-second-carry-on regression is caught instead by a
    // ceiling derived from the burst's own measured duration, which holds no matter
    // how long it actually took.
    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 500; ++i) {
        sink.process(event_of("burst message crossing the sixty four byte cap repeatedly"));
    }
    sink.flush();
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // At most one file per distinct second the burst touched, plus one for the second
    // it started in. A regression that fans out into a file per rotation attempt (e.g.
    // next_path()'s timestamp branch falling through into indexed-naming below it)
    // produces on the order of hundreds of files here -- far past this bound on any
    // machine, since the burst itself never legitimately takes hundreds of seconds.
    const auto max_files = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() + 2;
    EXPECT_LE(files_in_dir(), static_cast<std::size_t>(max_files));

    // Secondary, non-load-bearing check: documents that carrying on within the same
    // second must never produce two files for one timestamp. The ceiling above is
    // what actually catches a fan-out regression -- this holds trivially either way,
    // since on-disk filenames are unique by construction.
    std::set<std::string> timestamps;
    for (const auto& entry : fs::directory_iterator{dir}) {
        timestamps.insert(embedded_timestamp(entry.path()));
    }
    EXPECT_EQ(timestamps.size(), files_in_dir());

    EXPECT_GT(fs::file_size(sink.file_path()), 64U);
    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
}

TEST_F(RotationTest, InitialScanIgnoresADirectoryShapedLikeARotatedFile) {
    // The construction-time counterpart to FailedRotationKeepsTheCurrentFileAndGoesDegraded
    // below: initial_path()'s resume-from-highest-index scan must not mistake a directory
    // sharing a rotated file's name for a resumable candidate. Before the fix, this made
    // the sink open Dead with ec clear, and every janitor retry re-resolved the very same
    // directory -- unrecoverable, not merely degraded.
    fs::create_directories(dir / "app_1.log");

    FileSink<LightEntry> sink{config(true, false, 128)};

    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
    EXPECT_EQ(sink.file_path(), dir / "app.log");

    sink.process(event_of("resumed cleanly, ignoring the directory obstacle"));
    sink.flush();
    EXPECT_GT(fs::file_size(dir / "app.log"), 0U);
}

TEST_F(RotationTest, FailedRotationKeepsTheCurrentFileAndGoesDegraded) {
    FileSink<LightEntry> sink{config(true, false, 128)};
    // Created after construction -- not because it would derail the initial open (it no
    // longer does; see InitialScanIgnoresADirectoryShapedLikeARotatedFile above), but to
    // keep this test isolated to the rotation-time obstacle it targets.
    fs::create_directories(dir / "app_1.log");  // a directory where the next index must go

    for (int i = 0; i < 50; ++i) {
        sink.process(event_of("a message long enough to cross the rotation threshold"));
    }
    sink.flush();

    EXPECT_EQ(sink.get_status(), SinkStatus::Degraded);
    EXPECT_EQ(sink.file_path(), dir / "app.log");
    EXPECT_GT(fs::file_size(dir / "app.log"), 128U);  // still writing, nothing lost
    EXPECT_FALSE(sink.last_error().empty());
}

TEST_F(RotationTest, DegradedSinkRotatesOnceTheObstacleIsGone) {
    FileSink<LightEntry> sink{config(true, false, 128)};
    // See FailedRotationKeepsTheCurrentFileAndGoesDegraded for why this is created
    // after construction rather than before.
    fs::create_directories(dir / "app_1.log");

    for (int i = 0; i < 50; ++i) {
        sink.process(event_of("a message long enough to cross the rotation threshold"));
    }
    ASSERT_EQ(sink.get_status(), SinkStatus::Degraded);

    fs::remove(dir / "app_1.log");
    sink.force_maintain();

    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
    EXPECT_EQ(sink.file_path(), dir / "app_1.log");
}
