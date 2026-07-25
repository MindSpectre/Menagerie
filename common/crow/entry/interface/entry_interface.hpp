#pragma once

#include <cinttypes>
#include <menagerie/beavers>
#include <menagerie/chrono>
#include <source_location>
#include <thread>


#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
        #include <unistd.h>
    #else
        #include <sys/syscall.h>
    #endif
#endif

#include "detail/log_level.hpp"

namespace menagerie::crow {
    /// Fixed-capacity storage for class/subsystem log prefixes.
    ///
    /// Capacity is 31 chars + null terminator. Chosen to keep the struct
    /// compact for per-event copies in the hot path while fitting common
    /// class names (e.g. "PostgresAsyncExecutor" = 21 chars).
    ///
    /// Overflow behavior (see beavers::InlineString::assign):
    /// - consteval context (literal via SCROLL_COMPONENT_PREFIX): compile error
    /// - runtime context (set_prefix / dynamic source): silently truncates
    using PrefixNameStorage = beavers::InlineString<31>;

    /// Implementation details of the entry model: the Meta* mixins EntryBase composes
    /// from, and the machinery make_entry() (entry_factory.hpp) uses to build only the
    /// mixins a given EntryT declares via entry_traits.
    namespace detail {
        /// Maps an EntryT to the Meta* mixins it wants; specialized once per EntryType
        /// (see DetailedEntry, LightEntry) so make_entry() only constructs those mixins.
        template <class EntryT>
        struct entry_traits;

        /// Empty mixin: an entry with no metadata beyond level and message (see LightEntry).
        struct MetaNone {};  // 0-B

        /// Mixin capturing the call site (source_location) an entry was logged from.
        struct MetaSource {
            std::source_location location;  ///< Call site the entry was logged from.

            MetaSource() = default;

            /// Captures loc as the entry's call site.
            constexpr explicit MetaSource(const std::source_location& loc) noexcept
                : location{loc} {
            }
        };

        /// Mixin capturing the logging class/subsystem prefix set via LoggerProvider.
        struct MetaPrefix {
            PrefixNameStorage prefix;  ///< Class/subsystem prefix, truncated to capacity.
            MetaPrefix() = default;
            /// Stores sv as the entry's prefix, truncating if it exceeds PrefixNameStorage's capacity.
            constexpr explicit MetaPrefix(const std::string_view sv) noexcept {
                prefix.assign(sv);
            }
        };

        /// Reads the calling thread's OS-level thread id (gettid() on modern glibc,
        /// a SYS_gettid syscall on older glibc, a hashed std::thread::id elsewhere).
        [[nodiscard]] inline uint64_t capture_kernel_tid() noexcept {
#if defined(__linux__)
    #if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 30))
            return static_cast<uint64_t>(::gettid());
    #else
            return static_cast<uint64_t>(::syscall(SYS_gettid));
    #endif
#else
            return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
        }

        /// Per-thread tid/pid captured once and reused by MetaThread/MetaProcess, so
        /// building an entry never repeats the syscalls capture_kernel_tid()/getpid() need.
        struct ThreadLocalCache {
            uint64_t tid;         ///< OS-level thread id, from capture_kernel_tid().
            int32_t pid;          ///< Process id, from getpid().
            char tid_str[16]{};   ///< tid pre-rendered as a decimal C string.
            char pid_str[16]{};   ///< pid pre-rendered as a decimal C string.

            ThreadLocalCache() noexcept {
                tid = capture_kernel_tid();
                pid = getpid();
                snprintf(tid_str, sizeof(tid_str), "%" PRIu64, tid);
                snprintf(pid_str, sizeof(pid_str), "%d", pid);
            }
        };

        /// Calling thread's cached tid/pid, populated once per thread on first use.
        inline thread_local ThreadLocalCache tl_cache;

        /// Mixin capturing the logging thread's id, copied from the cached tl_cache.
        struct MetaThread {
            uint64_t tid;        ///< OS-level thread id, copied from tl_cache.
            char tid_str[16]{};  ///< tid pre-rendered as a decimal C string.

            MetaThread() noexcept
                : tid{tl_cache.tid} {
                std::memcpy(tid_str, tl_cache.tid_str, sizeof(tid_str));
            }
        };

        /// Mixin capturing the process id, copied from the cached tl_cache.
        struct MetaProcess {
            int32_t pid;         ///< Process id, copied from tl_cache.
            char pid_str[16]{};  ///< pid pre-rendered as a decimal C string.

            MetaProcess() noexcept
                : pid{tl_cache.pid} {
                std::memcpy(pid_str, tl_cache.pid_str, sizeof(pid_str));
            }
        };

        /// Mixin capturing the wall-clock time an entry was constructed.
        struct MetaTimePoint {
            std::chrono::time_point<std::chrono::system_clock> time_point;  ///< Construction-time timestamp.

            MetaTimePoint() noexcept
                : time_point{chrono::Clock::now()} {
            }
        };

        /// Base every EntryType derives from: stores level and message, and composes in
        /// whichever Meta* mixins the EntryType lists (via public inheritance from Metas...).
        /// A derived EntryType overrides format_into() to render itself into a string.
        template <class... Metas>
        class EntryBase : public Metas... {
        public:
            /// Constructs from a level, message, and one instance of each mixin in Metas.
            constexpr EntryBase(const LogLevel lvl,
                                const std::string_view msg,
                                Metas... metas)  // perfect-forward meta-packs
                : Metas{std::move(metas)}...,
                  level_{lvl},
                  message_{msg} {
            }

            /// The entry's severity level.
            [[nodiscard]] constexpr LogLevel level() const noexcept {
                return level_;
            }

            /// The entry's message text.
            [[nodiscard]] constexpr std::string_view message() const noexcept {
                return message_;
            }

            virtual ~EntryBase()                             = default;
            EntryBase()                                      = default;

            /// Formats this entry (level, message, and whichever Meta* mixins it carries)
            /// into out, replacing out's previous contents.
            virtual void format_into(std::string& out) const = 0;

        protected:
            LogLevel level_{LogLevel::Debug};  ///< This entry's severity level.
            std::string message_;              ///< This entry's message text.
            /// 3-letter codes indexed by LogLevel, e.g. level_strings[LogLevel::Warning] -> "WRN".
            static constexpr std::array<const char*, 6> level_strings = {"TRC", "DBG", "INF", "WRN", "ERR", "FAT"};

            /// Returns level_'s 3-letter code (e.g. "WRN"), looked up in level_strings.
            [[nodiscard]] constexpr const char* level_cstr() const noexcept {
                return level_strings[static_cast<std::size_t>(level_)];
            }
        };

        /// Satisfied by any type with EntryBase's level()/message()/format_into() surface,
        /// regardless of which Meta* mixins it composes.
        template <typename T>
        concept EntryConcept = requires(const T& entry) {
            { entry.level() } -> std::same_as<LogLevel>;
            { entry.message() } -> std::same_as<std::string_view>;
            { entry.format_into(std::declval<std::string&>()) } -> std::same_as<void>;
        };
    }  // namespace detail
}  // namespace menagerie::crow
