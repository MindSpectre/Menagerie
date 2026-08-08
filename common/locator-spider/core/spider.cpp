#include "spider.hpp"

#include <iostream>
#include <menagerie/chrono>

namespace menagerie::spider {
    Spider::Spider() noexcept
        : janitor_([this] { sweep_loop(); }) {
        std::cout << "[Spider] Constructed\t" << chrono::LocalClock::current_time(chrono::clock_formats::iso8601)
                  << "\n";
        // Todo: replace with logger?
    }

    Spider::~Spider() noexcept {
        stop_.store(true, std::memory_order_relaxed);
        if (janitor_.joinable())
            janitor_.join();  // graceful
        std::cout << "[Spider] Destructed\t" << chrono::LocalClock::current_time(chrono::clock_formats::iso8601)
                  << "\n";
        // Todo: replace with logger?
    }

    void Spider::clear() noexcept {
        std::unique_lock lk{mtx_};
        map_.clear();
    }

    // -------- janitor helpers --------

    void Spider::sweep_loop() {
        while (!stop_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(sweep_interval_.load());
            sweep();
        }
    }

    void Spider::sweep() {
        const auto now = std::chrono::steady_clock::now();
        std::unique_lock w{mtx_};

        for (auto it = map_.begin(); it != map_.end();) {
            const bool should_clear = std::visit(
                [&]<typename U>(U& tag) {
                    using Tag = std::decay_t<U>;

                    if constexpr (std::is_same_v<Tag, Scoped>) {
                        // Remove entirely when only we hold it
                        if (it->second.obj && it->second.obj.use_count() == 1) {
                            return true;  // Will erase the slot
                        }
                    } else if constexpr (std::is_same_v<Tag, Timed>) {
                        if (now - it->second.last_touch > tag.idle) {
                            // Just clear the object, keep the factory
                            it->second.obj.reset();
                            // Don't erase slot, just clear object for re-creation
                        }
                    }
                    return false;
                },
                it->second.lt);

            if (should_clear) {
                it = map_.erase(it);
            } else {
                ++it;
            }
        }
    }
}  // namespace menagerie::spider
