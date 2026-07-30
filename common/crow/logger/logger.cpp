#include "logger.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <print>

namespace menagerie::crow {
    void Logger::add_sink(std::shared_ptr<Sink> sink) {
        {
            std::lock_guard lock{registry_mutex_};
            auto next = std::make_shared<SinkTable>(*sinks_);
            next->push_back(SinkSlot{std::move(sink), boost::asio::make_strand(executor_)});
            sinks_ = std::move(next);
            publish_gate(*sinks_);
        }
        registry_version_.fetch_add(1, std::memory_order_release);
    }

    bool Logger::remove_sink(const std::shared_ptr<Sink>& sink) {
        {
            std::lock_guard lock{registry_mutex_};
            const auto found = std::ranges::find(*sinks_, sink, &SinkSlot::sink);
            if (found == sinks_->end()) {
                return false;
            }
            auto next = std::make_shared<SinkTable>(*sinks_);
            next->erase(next->begin() + std::distance(sinks_->begin(), found));
            sinks_ = std::move(next);
            publish_gate(*sinks_);
        }
        registry_version_.fetch_add(1, std::memory_order_release);
        return true;
    }

    std::vector<SinkReport> Logger::sink_report() const {
        const auto sinks = snapshot();
        std::vector<SinkReport> report;
        report.reserve(sinks->size());
        for (const auto& [sink, strand] : *sinks) {
            report.push_back(SinkReport{.sink        = sink,
                                        .status      = sink->get_status(),
                                        .undelivered = sink->undelivered(),
                                        .last_error  = sink->last_error()});
        }
        return report;
    }

    void Logger::publish_event(const LogLevel lvl,
                               const std::string_view prefix,
                               std::string& msg,
                               const std::source_location& loc) {
        const auto meta = EventMeta{lvl, loc};

        const std::int64_t seq = disruptor_.sequencer().next();
        auto& event            = disruptor_.ring_buffer()[seq];

        event.message.swap(msg);
        event.prefix.assign(prefix);
        apply_meta(event, meta);

        disruptor_.sequencer().publish(seq);
    }

    void Logger::shutdown() {
        if (!running_.load(std::memory_order_acquire)) {
            return;  // Already shut down
        }

        // 0. Stop the janitor before anything else: once the shutdown sentinel is in the
        //    ring, no maintain() call may still be posted to a sink's strand.
        janitor_.request_stop();
        if (janitor_.joinable()) {
            janitor_.join();
        }

        // 1. Send shutdown signal through disruptor
        const std::int64_t seq                        = disruptor_.sequencer().next();
        disruptor_.ring_buffer()[seq].shutdown_signal = true;
        disruptor_.sequencer().publish(seq);

        // 2. Wait for consumer loop to exit (drains ring buffer, posts to strands)
        if (consumer_thread_.joinable()) {
            consumer_thread_.join();
        }

        // 3. Drain all pending strand work, then flush each sink.
        //    Since strands are serial, this flush runs AFTER all previously
        //    posted process() calls complete -- no events are lost.
        const auto sinks = snapshot();
        if (!sinks->empty()) {
            std::atomic<std::size_t> remaining{sinks->size()};
            std::promise<void> done;

            for (const auto& [sink, strand] : *sinks) {
                boost::asio::post(strand, [&sink, &remaining, &done] {
                    sink->flush();
                    if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                        done.set_value();
                    }
                });
            }

            done.get_future().wait();
        }

        // 4. Shutdown internal pool if we own it
        if (owned_pool_) {
            owned_pool_->join();
        }

        running_.store(false, std::memory_order_release);
    }
    void Logger::consumer_loop(const std::stop_token& token) {
        std::int64_t next_seq                  = 0;
        // Load the version BEFORE taking the snapshot (mirrors the in-loop recheck below).
        // cached_version must never be newer than the table sinks actually holds: if a
        // mutation completed between these two reads, cached_version would be newer than
        // sinks, and every later version check would see no change -- staleness that
        // never self-heals until an unrelated mutation happens to bump the version again.
        // Reading version first means the worst case is the opposite: sinks is at least as
        // fresh as cached_version, so a mutation landing in the gap is simply seen again on
        // the very next check, at worst costing one extra re-lock.
        std::uint32_t cached_version           = registry_version_.load(std::memory_order_acquire);
        std::shared_ptr<const SinkTable> sinks = snapshot();

        while (running_.load(std::memory_order_acquire) && !token.stop_requested()) {
            const std::int64_t available =
                disruptor_.sequencer().get_highest_published(next_seq, disruptor_.sequencer().get_cursor());

            if (available < next_seq) {
                // Nothing published yet -- back off using the configured wait strategy
                // (BusySpin / Yielding / Blocking) instead of tight-spinning
                std::ignore = disruptor_.sequencer().wait_for(next_seq);
                continue;
            }

            // Collect batch: move all available events out of ring buffer
            auto batch = std::make_shared<std::vector<LogEvent>>();
            batch->reserve(static_cast<std::size_t>(available - next_seq + 1));

            auto max_lvl               = LogLevel::Trace;
            std::int64_t last_consumed = next_seq - 1;
            for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                auto& event = disruptor_.ring_buffer()[seq];

                if (event.shutdown_signal) {
                    last_consumed = seq;
                    running_.store(false, std::memory_order_release);
                    break;
                }

                max_lvl = std::max(max_lvl, event.level);  // free: the move below reads the whole event
                batch->emplace_back(std::move(event));
                last_consumed = seq;
            }

            // Bulk gating update -- producers see all freed slots at once
            disruptor_.sequencer().update_gating_sequence(last_consumed);
            next_seq = last_consumed + 1;

            // Post batch to each sink's strand (one post per sink, not per event)
            if (!batch->empty()) {
                // Version read before snapshot -- see the invariant comment at this
                // function's top. Worst case here is a false-positive mismatch (one
                // extra re-lock), never silent staleness.
                if (const auto version = registry_version_.load(std::memory_order_acquire); version != cached_version) {
                    sinks          = snapshot();
                    cached_version = version;
                }
                for (const auto& [sink, strand] : *sinks) {
                    const auto hint = sink->dispatch_hint();  // one relaxed load: status + threshold
                    if (max_lvl < hint.threshold) {
                        continue;
                    }
                    if (hint.status == SinkStatus::Dead) {
                        sink->add_undelivered(batch->size());
                        continue;
                    }
                    boost::asio::post(strand, [sink, batch] { sink->process_batch(batch); });
                }
            }
        }
    }

    void Logger::janitor_loop(const std::stop_token& token) {
        std::condition_variable_any cv;
        std::mutex mtx;

        while (!token.stop_requested()) {
            {
                std::unique_lock lock{mtx};
                cv.wait_for(lock, token, health_check_interval_, [] { return false; });
            }
            if (token.stop_requested()) {
                break;
            }
            sweep_once();
        }
    }

    void Logger::sweep() {
        sweep_once();
    }

    void Logger::set_error_callback(std::function<void(const SinkFailure&)> callback) {
        std::lock_guard lock{sweep_mutex_};
        error_callback_ = std::move(callback);
    }

    std::vector<Logger::PendingTransition> Logger::report_transitions(const SinkTable& table) {
        std::vector<PendingTransition> transitions;
        for (const auto& [sink, strand] : table) {
            const auto status            = sink->get_status();
            const auto [entry, inserted] = reported_.try_emplace(sink.get(), SinkStatus::Healthy);
            if (!inserted && entry->second == status) {
                continue;
            }
            const SinkStatus from = entry->second;
            entry->second         = status;
            if (inserted && status == SinkStatus::Healthy) {
                continue;  // first sight of a healthy sink is not a transition
            }

            transitions.emplace_back(sink.get(), from, status, sink->last_error(), sink->undelivered());
        }
        return transitions;
    }

    void Logger::default_error_report(const SinkFailure& failure) {
        // Not const: the covered branch below hands this to publish_event(), which
        // swaps it into the ring slot rather than copying it.
        std::string message = std::format("sink {} {} -> {}: {} ({} events undelivered)",
                                          static_cast<const void*>(failure.sink),
                                          to_string(failure.from),
                                          to_string(failure.to),
                                          failure.reason.empty() ? "no reason recorded" : failure.reason,
                                          failure.undelivered);

        const auto sinks   = snapshot();
        const bool covered = std::ranges::any_of(*sinks, [](const SinkSlot& slot) {
            return slot.sink->get_status() != SinkStatus::Dead && slot.sink->should_log(LogLevel::Error, "crow");
        });

        if (covered) {
            // Acceptance already proven above: publish directly rather than through
            // log(), which would re-run (and could fail) the same gate check.
            publish_event(LogLevel::Error, "crow", message, std::source_location::current());
            return;
        }
        std::println(std::cerr, "[crow] {}", message);
    }

    void Logger::sweep_once() {
        // Lock order: sweep_mutex_ (outer) then registry_mutex_ (inner, inside
        // republish_gate_from_registry() and snapshot()). add_sink()/remove_sink() never
        // touch sweep_mutex_, so there is no path that takes these two in the opposite
        // order -- no inversion is possible.
        std::vector<PendingTransition> transitions;
        std::function<void(const SinkFailure&)> callback;
        // Declared at function scope (not inside the locked block below) so the table --
        // and every sink it keeps alive -- outlives the callback loop after the lock is
        // released. A sink that just transitioned to Healthy has no maintain() post
        // holding its own shared_ptr (only non-Healthy sinks get posted below), so without
        // this, a concurrent remove_sink() plus the caller dropping its own shared_ptr
        // could free the sink between the closing brace and the callback loop, dangling
        // the SinkFailure::sink pointer logger.hpp documents as valid for that duration.
        {
            std::lock_guard lock{sweep_mutex_};

            // Publishes from the live registry table under registry_mutex_, not from a
            // snapshot taken earlier -- see its doc comment for why that distinction matters.
            republish_gate_from_registry();

            std::shared_ptr<const SinkTable> sinks = snapshot();
            transitions                            = report_transitions(*sinks);
            callback                               = error_callback_;

            const auto now_ms = detail::steady_now_ms();
            for (const auto& [sink, strand] : *sinks) {
                if (const auto hint = sink->dispatch_hint();
                    hint.status != SinkStatus::Healthy && retry_due(hint, now_ms)) {
                    boost::asio::post(strand, [sink] { sink->maintain(); });
                }
            }

            std::erase_if(reported_, [&sinks](const auto& entry) {
                return std::ranges::none_of(*sinks,
                                            [&entry](const SinkSlot& slot) { return slot.sink.get() == entry.first; });
            });
        }

        // Invoked only after sweep_mutex_ is released: a callback that calls back into
        // sweep() would deadlock on this non-recursive mutex, and one that calls
        // add_sink()/remove_sink() would otherwise take registry_mutex_ while
        // sweep_mutex_ was still held. Legal by the established lock order, but there is
        // no reason to make user code rely on that.
        for (const auto& [sink, from, to, reason, undelivered] : transitions) {
            const SinkFailure failure{
                .sink = sink, .from = from, .to = to, .reason = reason, .undelivered = undelivered};
            try {
                if (callback) {
                    callback(failure);
                } else {
                    default_error_report(failure);
                }
            } catch (...) {
                // A reporting handler must never take the janitor thread down with it.
            }
        }
    }
}  // namespace menagerie::crow
