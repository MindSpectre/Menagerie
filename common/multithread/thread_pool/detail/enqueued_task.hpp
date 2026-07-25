#pragma once
#include <functional>

namespace menagerie::multithread {
    class ThreadPool;

    /// A type-erased task plus its priority. `operator<` is a plain ascending
    /// comparison on `priority_`, and `std::priority_queue` is a max-heap over
    /// that ordering, so a *higher* `priority_` value pops first.
    class EnqueuedTask {
    public:
        /// Invokes the wrapped task, if any (a default-constructed `task` is a no-op).
        void execute() const {
            if (task) {
                task();
            }
        }

        /// Wraps `task` with its scheduling `priority` (higher runs first).
        EnqueuedTask(std::function<void()> task, const uint32_t priority)
            : task{std::move(task)},
              priority_{priority} {
        }

    private:
        std::function<void()> task;
        uint32_t priority_{1};

        friend bool operator<(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return lhs.priority_ < rhs.priority_;  // Ascending order; the priority_queue max-heap pops the highest priority_ first
        }

        friend bool operator<=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return !(lhs > rhs);
        }

        friend bool operator>(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return lhs.priority_ > rhs.priority_;
        }

        friend bool operator>=(const EnqueuedTask& lhs, const EnqueuedTask& rhs) {
            return !(lhs < rhs);
        }
    };
}  // namespace menagerie::multithread
