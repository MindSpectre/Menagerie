#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <list>
#include <menagerie/beavers>
#include <mutex>
#include <queue>
#include <thread>

#include <thread_safe_resource.hpp>

#include "config/thread_pool_config.hpp"
#include "detail/enqueued_task.hpp"

/// Growable thread pool, futex-based resource pools, a lock-free disruptor ring
/// buffer, and the small supporting utilities (mutex-wrapped resource access,
/// io_context runners, pause/pin helpers) they are built on.
namespace menagerie::multithread {
    /**
     * @brief Priority-queue-backed worker pool that grows between
     *        `min_threads` and `max_threads` and reaps idle workers past
     *        `idle_timeout` on a background cleanup thread.
     *
     * `enqueue(func, priority, args...)` returns a `std::future` for the
     * result; a new worker is spun up when the pool is not yet full and no
     * worker is idle. Unlike the other primitives in this library, ThreadPool
     * has an out-of-line `.cpp` rather than being header-only.
     */
    class ThreadPool : beavers::Immutable {
    public:
        using TaskPriority = uint32_t;  ///< Higher values run first in the task priority queue.

        /// @throw std::invalid_argument if `config.ok()` is false.
        explicit ThreadPool(const ThreadPoolConfig& config) {
            if (!config.ok()) {
                throw std::invalid_argument("Invalid config");
            }
            config_ = config;
            for (std::size_t i = 0; i < min_threads(); ++i) {
                create_worker();
            }
            if (config_.enable_cleanup_thread) {
                start_cleanup_thread();
            }
        }

        /// Calls shutdown() and joins every worker and the cleanup thread.
        ~ThreadPool();

        /// Queues `f(args...)` at `task_priority` (higher runs first) and
        /// returns a future for its result. May spin up a new worker.
        /// @throw std::runtime_error if the pool has already been shut down.
        template <class Func, class... Args>
        std::future<std::invoke_result_t<Func, Args...>>
        enqueue(Func&& f, TaskPriority task_priority = 1, Args&&... args);

        /// Stops accepting new work, wakes and joins every worker and the
        /// cleanup thread. Safe to call more than once.
        void shutdown();


        /// True until shutdown() has been called.
        [[nodiscard]] bool is_running() const {
            return !stop_;
        }

        /// Current worker count.
        [[nodiscard]] std::size_t size() const {
            return workers_.read()->size();
        }

        /// Configured lower bound on worker count.
        [[nodiscard]] std::size_t min_threads() const {
            return config_.min_threads;
        }

        /// Configured upper bound on worker count.
        [[nodiscard]] std::size_t max_threads() const {
            return config_.max_threads;
        }

        /// Workers currently executing a task.
        [[nodiscard]] std::size_t active_threads() const {
            return active_threads_.load();
        }

        /// Workers alive but not currently executing a task.
        [[nodiscard]] std::size_t free_threads() const {
            return size() - active_threads();
        }

        /// Returns the configuration this pool was constructed with.
        [[nodiscard]] const ThreadPoolConfig& config() const {
            return config_;
        }

        /// True iff `size() == max_threads()`.
        [[nodiscard]] bool is_full() const {
            return size() == max_threads();
        }

        /// Configured idle time before a worker above min_threads is reaped.
        [[nodiscard]] std::chrono::milliseconds idle_timeout() const {
            return config_.idle_timeout;
        }

        /// Configured period of the background cleanup thread.
        [[nodiscard]] std::chrono::milliseconds cleanup_interval() const {
            return config_.cleanup_interval;
        }

    private:
        struct safe_thread {
            std::atomic<bool> valid{true};
            std::jthread thread;
        };

        void create_worker();
        void start_cleanup_thread();
        void cleanup_invalid_workers();
        ThreadSafeResource<std::list<safe_thread>> workers_;
        ThreadSafeResource<std::priority_queue<EnqueuedTask>> tasks_;

        std::mutex task_queue_mutex_;
        std::condition_variable task_condition_;

        std::jthread cleanup_thread_;
        std::condition_variable cleanup_condition_;
        std::mutex cleanup_mutex_;

        std::atomic<bool> stop_{false};

        ThreadPoolConfig config_{};
        std::atomic<size_t> active_threads_{0};
    };
}  // namespace menagerie::multithread

template <class Func, class... Args>
std::future<std::invoke_result_t<Func, Args...>>
menagerie::multithread::ThreadPool::enqueue(Func&& f, TaskPriority task_priority, Args&&... args) {
    using return_type = std::invoke_result_t<Func, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [func = std::forward<Func>(f), ... args = std::forward<Args>(args)]() mutable {
            return std::invoke(std::move(func), std::move(args)...);
        });

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock lock(task_queue_mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool is stopped");
        }
        tasks_.write()->emplace([task] { (*task)(); }, task_priority);

        cleanup_invalid_workers();
        // Create worker if needed and we haven't reached max threads
        // TODO: Issue#33
        if (!is_full() && !free_threads()) {
            create_worker();
        }
    }
    task_condition_.notify_one();
    return res;
}
