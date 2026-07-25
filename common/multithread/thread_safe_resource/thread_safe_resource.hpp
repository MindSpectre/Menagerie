#pragma once
#include <mutex>
#include <shared_mutex>

namespace menagerie::multithread {
    /**
     * @brief `std::shared_mutex` wrapper around a `T`: `.read()` returns a
     *        `ReadProxy` holding a shared lock, `.write()` (and `operator->`)
     *        return a `WriteProxy` holding a unique lock, and `with_lock` /
     *        `with_read_lock` take a callable for scoped access.
     *
     * The proxy holds the lock for its lifetime - keep it short-lived and
     * do not store it past the statement that acquired it.
     */
    template <typename T>
    class ThreadSafeResource {
    public:
        /// RAII shared-lock handle over the wrapped resource, returned by `read()`.
        class ReadProxy {
        public:
            /// Acquires the shared lock over `mutex` and binds to `resource`.
            ReadProxy(std::shared_mutex& mutex, const T& resource)
                : lock_{mutex},
                  resource_{resource} {
            }

            /// Accesses a member of the wrapped resource.
            const T* operator->() const {
                return &resource_;
            }
            /// Dereferences the wrapped resource.
            const T& operator*() const {
                return &resource_;
            }

        private:
            std::shared_lock<std::shared_mutex> lock_;
            const T& resource_;
        };

        /// RAII exclusive-lock handle over the wrapped resource, returned by `write()`.
        class WriteProxy {
        public:
            /// Acquires the exclusive lock over `mutex` and binds to `resource`.
            WriteProxy(std::shared_mutex& mutex, T& resource)
                : lock_{mutex},
                  resource_{resource} {
            }

            /// Accesses a member of the wrapped resource.
            T* operator->() {
                return &resource_;
            }
            /// Const overload: accesses a member of the wrapped resource.
            const T* operator->() const {
                return &resource_;
            }

            /// Dereferences the wrapped resource.
            T& operator*() {
                return resource_;
            }
            /// Const overload: dereferences the wrapped resource.
            const T& operator*() const {
                return &resource_;
            }

        private:
            std::unique_lock<std::shared_mutex> lock_;
            T& resource_;
        };

        /// Forwards `args...` to `T`'s constructor.
        template <typename... Args>
        explicit ThreadSafeResource(Args&&... args)
            : resource_(std::forward<Args>(args)...) {
        }

        /// @copydoc write
        WriteProxy operator->() {
            return WriteProxy(mutex_, resource_);
        }

        /// Acquires the exclusive lock and returns a `WriteProxy` over the resource.
        WriteProxy write() {
            return WriteProxy(mutex_, resource_);
        }

        /// Acquires the shared lock and returns a `ReadProxy` over the resource.
        ReadProxy read() const {
            return ReadProxy(mutex_, resource_);
        }

        /// @copydoc read
        ReadProxy operator->() const {
            return ReadProxy(mutex_, resource_);
        }

        /// Runs `func(resource)` under the exclusive lock and returns its result.
        template <typename Func>
            requires std::is_invocable_v<Func, T&>
        std::invoke_result_t<Func, T&> with_lock(Func&& func) {
            std::unique_lock lock(mutex_);
            return func(resource_);
        }

        /// Runs `func(resource)` under the shared lock and returns its result.
        template <typename Func>
            requires std::is_invocable_v<Func, const T&>
        std::invoke_result_t<Func, const T&> with_read_lock(Func&& func) const {
            std::shared_lock lock(mutex_);
            return func(resource_);
        }


    private:
        mutable std::shared_mutex mutex_;  // Allow reader-writer locks
        T resource_;
    };


}  // namespace menagerie::multithread
