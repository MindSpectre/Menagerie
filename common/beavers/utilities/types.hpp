#pragma once

#include <string>
#include <typeindex>

namespace menagerie::beavers {
    /// Byte-size literal suffixes (`10_kb`, `4_mb`, ...), binary (1024-based) units.
    namespace literals {
        /// Byte (identity conversion).
        constexpr unsigned long long operator""_b(const unsigned long long value) {
            return value;
        }

        /// Kilobyte (1024 bytes).
        constexpr unsigned long long operator""_kb(const unsigned long long value) {
            return value * 1024ULL;
        }

        /// Megabyte (1024 KB).
        constexpr unsigned long long operator""_mb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL;
        }

        /// Gigabyte (1024 MB).
        constexpr unsigned long long operator""_gb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL * 1024ULL;
        }

        /// Terabyte (1024 GB).
        constexpr unsigned long long operator""_tb(const unsigned long long value) {
            return value * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
        }
    }  // namespace literals

    /// Compile-time type-name string for the common arithmetic / string types,
    /// falling back to `typeid(T).name()` (compiler-mangled) for everything else.
    template <typename T>
    constexpr const char* get_type_name() {
        if constexpr (std::is_same_v<T, int>)
            return "int";
        else if constexpr (std::is_same_v<T, double>)
            return "double";
        else if constexpr (std::is_same_v<T, float>)
            return "float";
        else if constexpr (std::is_same_v<T, bool>)
            return "bool";
        else if constexpr (std::is_same_v<T, std::string>)
            return "string";
        else if constexpr (std::is_same_v<T, std::int32_t>)
            return "int32";
        else if constexpr (std::is_same_v<T, std::int64_t>)
            return "int64";
        else
            return typeid(T).name();  // fallback to mangled name
    }

    /// Runtime counterpart of `get_type_name<T>()` keyed off a `std::type_index`.
    inline const char* get_type_name_from_index(const std::type_index& ti) {
        if (ti == std::type_index(typeid(int)))
            return "int";
        if (ti == std::type_index(typeid(double)))
            return "double";
        if (ti == std::type_index(typeid(float)))
            return "float";
        if (ti == std::type_index(typeid(bool)))
            return "bool";
        if (ti == std::type_index(typeid(std::string)))
            return "string";
        if (ti == std::type_index(typeid(std::int32_t)))
            return "int32";
        if (ti == std::type_index(typeid(std::int64_t)))
            return "int64";
        return ti.name();
        // fallback
    }
}  // namespace menagerie::beavers
