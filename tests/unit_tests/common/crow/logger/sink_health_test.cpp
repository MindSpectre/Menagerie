#include <menagerie/crow>
#include <sstream>
#include <thread>

#include <gtest/gtest.h>

using namespace menagerie::crow;

namespace {
    /// Builds a ConsoleSinkConfig pointing at a caller-owned stream.
    ConsoleSinkConfig console_config_for(std::ostream* out, const LogLevel threshold = DBG) {
        return ConsoleSinkConfig::Builder{}
            .threshold(threshold)
            .output(out)
            .enable_colors(false)
            .flush_each_entry(true)
            .finalize();
    }

    LogEvent make_event(const LogLevel level, const std::string& message) {
        LogEvent event;
        event.level   = level;
        event.message = message;
        return event;
    }

    /// Test-only sink for exercising the janitor's own retry_due gate in isolation.
    /// maintain() here never self-gates -- it just counts -- unlike FileSink::maintain(),
    /// which re-checks retry_due before doing any real work. Posting to a FileSink
    /// unconditionally therefore proves nothing about the janitor's gate: the sink's own
    /// gate would decline the work anyway and the outcome (still Dead) looks identical
    /// either way. ConsoleSink::maintain() has this exact no-self-gate shape in
    /// production (it only re-checks good()), so this probe isn't a contrived case --
    /// it's the case the janitor's gate is the *only* protection for.
    class ProbeSink final : public Sink {
    public:
        void process(const LogEvent&) noexcept override {
        }
        void flush() noexcept override {
        }
        [[nodiscard]] bool should_log(LogLevel, std::string_view) const noexcept override {
            return true;
        }

        void maintain() noexcept override {
            maintain_calls_.fetch_add(1, std::memory_order_relaxed);
        }

        [[nodiscard]] std::uint32_t maintain_calls() const noexcept {
            return maintain_calls_.load(std::memory_order_relaxed);
        }

        /// Drives Degraded via note_failure() n times, arming the same backoff a real
        /// sink's maintenance failures would (see detail::backoff_ms).
        void fail_times(const std::uint32_t n) noexcept {
            for (std::uint32_t i = 0; i < n; ++i) {
                note_failure();
            }
            set_status(SinkStatus::Degraded, "probe failure");
        }

    private:
        std::atomic<std::uint32_t> maintain_calls_{0};
    };
}  // namespace

TEST(ConsoleSinkHealthTest, StartsHealthy) {
    std::ostringstream out;
    const ConsoleSink<LightEntry> sink{console_config_for(&out)};
    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
}

TEST(ConsoleSinkHealthTest, BadStreamMarksSinkDegradedInsteadOfThrowing) {
    std::ostringstream out;
    ConsoleSink<LightEntry> sink{console_config_for(&out)};

    out.setstate(std::ios::badbit);  // stand-in for EPIPE on a closed pipe
    sink.process(make_event(INF, "after the stream broke"));

    EXPECT_EQ(sink.get_status(), SinkStatus::Degraded);
    EXPECT_FALSE(sink.last_error().empty());
}

TEST(ConsoleSinkHealthTest, RecoversWhenTheStreamIsUsableAgain) {
    std::ostringstream out;
    ConsoleSink<LightEntry> sink{console_config_for(&out)};

    out.setstate(std::ios::badbit);
    sink.process(make_event(INF, "broken"));
    ASSERT_EQ(sink.get_status(), SinkStatus::Degraded);

    out.clear();
    sink.maintain();

    EXPECT_EQ(sink.get_status(), SinkStatus::Healthy);
}

TEST(ConsoleSinkHealthTest, SetConfigRepublishesThreshold) {
    std::ostringstream out;
    ConsoleSink<LightEntry> sink{console_config_for(&out, DBG)};
    ASSERT_EQ(sink.dispatch_hint().threshold, DBG);

    sink.set_config(console_config_for(&out, ERR));

    EXPECT_EQ(sink.dispatch_hint().threshold, ERR);
}

#include <filesystem>

namespace fs = std::filesystem;

namespace {
    /// Path whose parent component is a regular file, so create_directories() fails
    /// with ENOTDIR. Works regardless of the user the tests run as — unlike chmod,
    /// which root ignores.
    class BlockedPath {
    public:
        explicit BlockedPath(std::string name)
            : blocker_{std::move(name)} {
            fs::remove_all(blocker_);
            std::ofstream{blocker_} << "not a directory";
        }

        ~BlockedPath() {
            fs::remove_all(blocker_);
        }

        [[nodiscard]] fs::path log_path() const {
            return blocker_ / "app.log";
        }

        void unblock() const {
            fs::remove(blocker_);
        }

    private:
        fs::path blocker_;
    };
}  // namespace

TEST(FileSinkHealthTest, UnwritablePathMarksSinkDeadInsteadOfTerminating) {
    const BlockedPath blocked{"blocked_ctor"};

    const FileSink<LightEntry> sink{FileSinkConfig::Builder{}
                                        .file(blocked.log_path())
                                        .add_time_to_filename(false)
                                        .rotate_file(false)
                                        .finalize()};

    EXPECT_EQ(sink.get_status(), SinkStatus::Dead);
    EXPECT_FALSE(sink.last_error().empty());
}

TEST(FileSinkHealthTest, DeadSinkDropsEventsAndCountsThem) {
    const BlockedPath blocked{"blocked_drop"};

    FileSink<LightEntry> sink{FileSinkConfig::Builder{}
                                  .file(blocked.log_path())
                                  .add_time_to_filename(false)
                                  .rotate_file(false)
                                  .finalize()};
    ASSERT_EQ(sink.get_status(), SinkStatus::Dead);

    auto batch = std::make_shared<std::vector<LogEvent>>();
    batch->push_back(make_event(INF, "one"));
    batch->push_back(make_event(INF, "two"));
    sink.process_batch(batch);

    EXPECT_EQ(sink.undelivered(), 2U);
    EXPECT_EQ(sink.get_status(), SinkStatus::Dead);
}

TEST(FileSinkHealthTest, MaintainReopensOnceThePathBecomesUsable) {
    const BlockedPath blocked{"blocked_recover"};

    FileSink<LightEntry> sink{FileSinkConfig::Builder{}
                                  .file(blocked.log_path())
                                  .add_time_to_filename(false)
                                  .rotate_file(false)
                                  .flush_each_entry(true)
                                  .finalize()};
    ASSERT_EQ(sink.get_status(), SinkStatus::Dead);

    blocked.unblock();
    sink.maintain();  // first retry is immediate (see detail::backoff_ms), so this recovers now
    ASSERT_EQ(sink.get_status(), SinkStatus::Healthy);

    sink.process(make_event(INF, "back online"));
    EXPECT_TRUE(fs::exists(sink.file_path()));

    fs::remove_all("blocked_recover");
}

namespace {
    /// Test-only EntryType: formats normally except for one sentinel message, where
    /// format_into() throws. FileSink is templated on EntryType exactly so a custom one
    /// can be dropped in like DetailedEntry/LightEntry are — this lets a *Healthy* sink be
    /// driven into Dead deterministically partway through a batch, exercising
    /// write_guarded()'s real catch path instead of a contorted production-code hook.
    class ThrowingEntry {
    public:
        ThrowingEntry(const LogLevel lvl, const std::string_view msg)
            : level_{lvl},
              message_{msg} {
        }

        [[nodiscard]] LogLevel level() const noexcept {
            return level_;
        }
        [[nodiscard]] std::string_view message() const noexcept {
            return message_;
        }

        void format_into(std::string& out) const {
            if (message_ == "boom") {
                throw std::runtime_error{"simulated formatting failure"};
            }
            out = message_;
        }

    private:
        LogLevel level_;
        std::string message_;
    };
}  // namespace

template <>
struct menagerie::crow::detail::entry_traits<ThrowingEntry> {
    using wants = menagerie::beavers::type_list<>;
};

TEST(FileSinkHealthTest, HealthySinkDyingMidBatchCountsTheRestAsUndelivered) {
    const fs::path log_path = "healthy_dies_mid_batch.log";
    fs::remove(log_path);

    FileSink<ThrowingEntry> sink{FileSinkConfig::Builder{}
                                     .file(log_path)
                                     .add_time_to_filename(false)
                                     .rotate_file(false)
                                     .finalize()};
    ASSERT_EQ(sink.get_status(), SinkStatus::Healthy);

    auto batch = std::make_shared<std::vector<LogEvent>>();
    batch->push_back(make_event(INF, "one"));
    batch->push_back(make_event(INF, "boom"));   // format_into() throws here: sink dies
    batch->push_back(make_event(INF, "three"));  // never reached
    batch->push_back(make_event(INF, "four"));   // never reached
    sink.process_batch(batch);

    EXPECT_EQ(sink.get_status(), SinkStatus::Dead);
    EXPECT_EQ(sink.undelivered(), 3U);  // "boom" plus the two events after it
    EXPECT_FALSE(sink.last_error().empty());

    fs::remove(log_path);
}

TEST(LoggerJanitorTest, SweepRecoversADeadFileSink) {
    BlockedPath blocked{"blocked_janitor"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    auto sink = std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                           .file(blocked.log_path())
                                                           .add_time_to_filename(false)
                                                           .rotate_file(false)
                                                           .flush_each_entry(true)
                                                           .finalize());
    logger.add_sink(sink);
    ASSERT_EQ(sink->get_status(), SinkStatus::Dead);

    blocked.unblock();
    logger.sweep();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(sink->get_status(), SinkStatus::Healthy);

    logger.log(INF, "written after recovery");
    logger.shutdown();

    EXPECT_TRUE(fs::exists(sink->file_path()));
    fs::remove_all("blocked_janitor");
}

TEST(LoggerJanitorTest, TickRecoversWithoutAnExplicitSweep) {
    BlockedPath blocked{"blocked_tick"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{50}).finalize()};
    auto sink = std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                           .file(blocked.log_path())
                                                           .add_time_to_filename(false)
                                                           .rotate_file(false)
                                                           .finalize());
    logger.add_sink(sink);
    ASSERT_EQ(sink->get_status(), SinkStatus::Dead);

    blocked.unblock();
    // The first retry is now immediate (see detail::backoff_ms), so recovery only
    // waits for the janitor's next 50ms tick plus a strand post -- not the 1s base
    // backoff. 300ms is 6x the tick interval, comfortable margin for CI jitter.
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(sink->get_status(), SinkStatus::Healthy);

    logger.shutdown();
    fs::remove_all("blocked_tick");
}

TEST(LoggerJanitorTest, SweepGatesMaintainOnTheJanitorsOwnRetryDeadline) {
    auto sink = std::make_shared<ProbeSink>();
    // Two failures push the deadline a full second out (backoff_ms(1) == 1000ms -- see
    // detail::backoff_ms), the same shape a real sink's maintenance failures produce.
    // (backoff_ms(0) == 0 would leave the very first retry unconditionally due, which is
    // exactly the blind spot that let this gate go untested for so long.)
    sink->fail_times(2);
    ASSERT_EQ(sink->maintain_calls(), 0U);

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    logger.add_sink(sink);

    logger.sweep();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));  // let any posted maintain() run
    // Not due yet: since ProbeSink::maintain() never declines on its own (unlike
    // FileSink, which re-checks retry_due internally), a nonzero count here can only mean
    // sweep_once() posted the work despite the deadline being in the future -- i.e. the
    // janitor's own gate, not the sink's, failed to hold.
    EXPECT_EQ(sink->maintain_calls(), 0U);

    std::this_thread::sleep_for(std::chrono::milliseconds(900));  // now past the ~1s deadline
    logger.sweep();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Due now: proves the counter is actually live rather than merely stuck at zero, so
    // the first assertion demonstrates the gate holding, not some unrelated wiring gap.
    EXPECT_EQ(sink->maintain_calls(), 1U);

    logger.shutdown();
}

TEST(LoggerJanitorTest, SweepRefreshesTheGateAfterSetConfig) {
    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    std::ostringstream out;
    auto sink = std::make_shared<ConsoleSink<LightEntry>>(console_config_for(&out, ERR));
    logger.add_sink(sink);
    ASSERT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(ERR));

    sink->set_config(console_config_for(&out, DBG));
    EXPECT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(ERR));  // documented staleness

    logger.sweep();
    EXPECT_EQ(logger.gate_threshold(), static_cast<std::uint8_t>(DBG));

    logger.shutdown();
}

TEST(SinkFailureReportTest, CustomCallbackSeesTheTransition) {
    BlockedPath blocked{"blocked_callback"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};

    std::vector<std::pair<SinkStatus, SinkStatus>> transitions;
    std::mutex transitions_mutex;
    logger.set_error_callback([&](const SinkFailure& failure) {
        std::lock_guard lock{transitions_mutex};
        transitions.emplace_back(failure.from, failure.to);
    });

    auto sink = std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                           .file(blocked.log_path())
                                                           .add_time_to_filename(false)
                                                           .rotate_file(false)
                                                           .finalize());
    logger.add_sink(sink);
    logger.sweep();  // first sweep observes Healthy -> Dead

    {
        std::lock_guard lock{transitions_mutex};
        ASSERT_EQ(transitions.size(), 1U);
        EXPECT_EQ(transitions[0].first, SinkStatus::Healthy);
        EXPECT_EQ(transitions[0].second, SinkStatus::Dead);
    }

    logger.sweep();  // no change: must not report again
    {
        std::lock_guard lock{transitions_mutex};
        EXPECT_EQ(transitions.size(), 1U);
    }

    logger.shutdown();
    fs::remove_all("blocked_callback");
}

TEST(SinkFailureReportTest, SinkOutlivesRemovalDuringItsOwnCallback) {
    BlockedPath blocked{"blocked_retention"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    auto sink = std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                           .file(blocked.log_path())
                                                           .add_time_to_filename(false)
                                                           .rotate_file(false)
                                                           .finalize());

    // The callback for the recovery transition unregisters the sink and drops this
    // test's own reference BEFORE touching failure.sink: the SinkFailure handle itself
    // must be what keeps the sink alive. The recovery transition is the load-bearing
    // choice -- a sink reported Healthy gets no maintain() post, so no strand closure
    // holds a stray shared_ptr that would mask a dangling handle here. Run under the
    // asan preset, this is the regression probe for SinkFailure's lifetime contract.
    SinkStatus status_seen{SinkStatus::Dead};
    bool callback_ran = false;
    logger.set_error_callback([&](const SinkFailure& failure) {
        if (failure.to != SinkStatus::Healthy) {
            return;  // the Healthy -> Dead report from the first sweep is not the subject
        }
        ASSERT_TRUE(logger.remove_sink(sink));
        sink.reset();
        status_seen  = failure.sink->get_status();
        callback_ran = true;
    });

    logger.add_sink(sink);
    ASSERT_EQ(sink->get_status(), SinkStatus::Dead);
    logger.sweep();  // reports Healthy -> Dead and posts a maintain() that fails again
    // Let that maintain() post drain: its strand closure holds its own shared_ptr to
    // the sink, and this test needs no such reference left by the time the callback
    // below drops the last one on purpose.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    blocked.unblock();
    sink->force_maintain();
    ASSERT_EQ(sink->get_status(), SinkStatus::Healthy);

    logger.sweep();  // reports Dead -> Healthy; the callback above runs on this thread
    ASSERT_TRUE(callback_ran);
    EXPECT_EQ(status_seen, SinkStatus::Healthy);

    logger.shutdown();
    fs::remove_all("blocked_retention");
}

TEST(SinkFailureReportTest, DefaultHandlerLogsThroughASurvivingSink) {
    BlockedPath blocked{"blocked_default"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    std::ostringstream console_out;
    logger.add_sink(std::make_shared<ConsoleSink<LightEntry>>(console_config_for(&console_out, TRC)));
    logger.add_sink(std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                               .file(blocked.log_path())
                                                               .add_time_to_filename(false)
                                                               .rotate_file(false)
                                                               .finalize()));

    logger.sweep();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    logger.shutdown();

    EXPECT_NE(console_out.str().find("DEAD"), std::string::npos);
    fs::remove_all("blocked_default");
}

TEST(SinkFailureReportTest, NoAcceptingSinkFallsBackToStderr) {
    BlockedPath blocked{"blocked_stderr"};

    Logger logger{LoggerConfig::Builder{}.health_check_interval(std::chrono::milliseconds{0}).finalize()};
    std::ostringstream console_out;
    // Threshold above Error: this sink exists but would filter the report out, so it
    // does not count as coverage and the report must go to stderr instead.
    logger.add_sink(std::make_shared<ConsoleSink<LightEntry>>(console_config_for(&console_out, FAT)));
    logger.add_sink(std::make_shared<FileSink<LightEntry>>(FileSinkConfig::Builder{}
                                                               .file(blocked.log_path())
                                                               .add_time_to_filename(false)
                                                               .rotate_file(false)
                                                               .finalize()));

    ::testing::internal::CaptureStderr();
    logger.sweep();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const std::string captured = ::testing::internal::GetCapturedStderr();
    logger.shutdown();

    EXPECT_NE(captured.find("DEAD"), std::string::npos);
    EXPECT_EQ(console_out.str().find("DEAD"), std::string::npos);
    fs::remove_all("blocked_stderr");
}
