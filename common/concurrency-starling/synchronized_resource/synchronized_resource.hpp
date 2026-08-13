#pragma once
#include <initializer_list>
#include <mutex>
#include <type_traits>
#include <utility>

namespace menagerie::starling {
    /**
     * @brief `std::mutex` wrapper around a `T`: `operator->` returns a `Proxy`
     *        holding the lock, and `in_lock` takes a callable for scoped access.
     *
     * Every access is exclusive, which is what makes this the default guard for
     * a shared `T`: a plain `std::mutex` acquisition is one uncontended atomic,
     * where a reader-writer lock costs several times that and bounces its reader
     * counter between cores. Reach for `SharedResource<T>`
     * (`shared_resource/shared_resource.hpp`) only when reads dominate and the
     * critical section is long enough to bury that overhead.
     *
     * The proxy holds the lock for its lifetime - keep it short-lived and do not
     * store it past the statement that acquired it. `std::mutex` is not
     * recursive, so two live proxies in one full expression -
     * `f(*res.lock(), *res.lock())`, or a second `res->` while a proxy is still
     * in scope - deadlock the calling thread; `in_lock` makes that scope
     * explicit and is the safer default.
     */
    template <typename T>
    class SynchronizedResource {
    public:
        /// RAII exclusive-lock handle over the wrapped resource, returned by `lock()`.
        ///
        /// `U` is `T` for a mutable resource and `const T` for a `const` one, so
        /// the view's constness rides on the handle's type. Returning a
        /// `const`-qualified handle over a plain `T&` would not do: `auto p =
        /// res.lock()` drops top-level `const` from the prvalue, handing the
        /// caller a mutable view of a `const` object.
        template <typename U>
        class BasicProxy {
        public:
            /// Acquires the exclusive lock over `mutex` and binds to `resource`.
            explicit BasicProxy(std::mutex& mutex, U& resource)
                : lock_{mutex},
                  resource_{resource} {
            }

            /// Accesses a member of the wrapped resource.
            U* operator->() const noexcept {
                return &resource_;
            }

            /// Dereferences the wrapped resource.
            U& operator*() const noexcept {
                return resource_;
            }

        private:
            std::unique_lock<std::mutex> lock_;
            U& resource_;
        };

        using Proxy      = BasicProxy<T>;
        using ConstProxy = BasicProxy<const T>;

        /// Forwards `args...` to `T`'s constructor: `T(args...)`.
        ///
        /// For scalar `T` the constraint also rejects narrowing, so
        /// `SynchronizedResource<int>{2.5}` stays ill-formed; use an explicit
        /// cast to truncate on purpose.
        template <typename... Args>
            requires std::is_constructible_v<T, Args...> &&
                     (!std::is_scalar_v<T> || requires { T{std::declval<Args>()...}; })
        explicit SynchronizedResource(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : resource_(std::forward<Args>(args)...) {
        }

        /// List-initializes `T` from `il`, mirroring `T{...}`: a braced
        /// `SynchronizedResource<std::vector<int>>{5}` holds one element, while
        /// the parenthesized form above holds five.
        template <typename U = T>
            requires requires { typename U::value_type; } &&
                     std::is_constructible_v<T, std::initializer_list<typename U::value_type>>
        SynchronizedResource(std::initializer_list<typename U::value_type> il)
            noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<typename U::value_type>>)
            : resource_(il) {
        }

        /// Acquires the lock and returns a `Proxy` over the resource.
        Proxy lock() {
            return Proxy{mutex_, resource_};
        }

        /// @copydoc lock
        ConstProxy lock() const {
            return ConstProxy{mutex_, resource_};
        }

        /// @copydoc lock
        Proxy operator->() {
            return Proxy{mutex_, resource_};
        }

        /// @copydoc lock
        ConstProxy operator->() const {
            return ConstProxy{mutex_, resource_};
        }

        /// Runs `func(resource)` under the lock and returns its result.
        template <typename Func>
            requires std::is_invocable_v<Func, T&>
        std::invoke_result_t<Func, T&> in_lock(Func&& func) {
            std::unique_lock lock{mutex_};
            return func(resource_);
        }

        /// Const overload: runs `func(resource)` under the lock and returns its result.
        template <typename Func>
            requires std::is_invocable_v<Func, const T&>
        std::invoke_result_t<Func, const T&> in_lock(Func&& func) const {
            std::unique_lock lock{mutex_};
            return func(resource_);
        }


    private:
        mutable std::mutex mutex_;
        T resource_;
    };


}  // namespace menagerie::starling
