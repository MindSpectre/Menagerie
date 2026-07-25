#pragma once

#include <thread>

#include <pthread.h>
#include <sched.h>

namespace menagerie::multithread {
    /// Pins the calling thread to `core` via `pthread_setaffinity_np`.
    /// @return false if the underlying `pthread_setaffinity_np` call failed.
    inline bool pin_current_thread_to_core(const int core) noexcept {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(core, &set);
        return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
    }
}  // namespace menagerie::multithread
