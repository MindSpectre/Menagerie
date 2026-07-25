#pragma once
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace menagerie::beavers {

    /**
     * @brief Compile-time string usable as a non-type template parameter.
     *
     * Stores the literal - including its trailing null - as a `char[N]`,
     * letting templates be specialised on string contents (e.g. field names).
     * Comparisons across different lengths are always false.
     */
    template <std::size_t N>
    struct FixedString {
        char data[N] = {};  ///< Raw literal storage, including the trailing null.

        /// Implicit constructor from a string literal - copies its `N` bytes,
        /// including the trailing null, into `data`.
        constexpr explicit(false) FixedString(const char (&str)[N]) noexcept {
            std::ranges::copy_n(str, N, data);
        }

        /// Explicit conversion to a `string_view` over the stored characters
        /// (excludes the trailing null).
        constexpr explicit operator std::string_view() const noexcept {
            return std::string_view{data, N - 1};
        }

        /// Returns a `string_view` over the stored characters (excludes the trailing null).
        [[nodiscard]] constexpr std::string_view view() const noexcept {
            return std::string_view{data, N - 1};
        }

        /// True iff the two strings hold identical content.
        constexpr bool operator==(const FixedString& other) const noexcept {
            return std::string_view{*this} == std::string_view{other};
        }

        /// Always false: `FixedString`s of different lengths are never equal.
        template <size_t M>
        constexpr bool operator==(const FixedString<M>&) const noexcept {
            return false;
        }
    };

    /// Deduction guide: infers `N` from the literal's array size.
    template <std::size_t N>
    FixedString(const char (&)[N]) -> FixedString<N>;


    /**
     * @brief Fixed-capacity, non-allocating string for constexpr SQL generation.
     *
     * Satisfies Appendable (operator+= for string_view and char).
     */
    template <std::size_t Capacity>
    struct InlineString {
        std::size_t size_ = 0;  ///< Current length in bytes.
        char data_[Capacity + 1]{};  ///< Character storage, `Capacity` bytes plus a null terminator.

        constexpr InlineString() noexcept = default;

        /// @throw std::out_of_range if capacity is exceeded
        constexpr InlineString& operator+=(const std::string_view sv) {
            if (size_ + sv.size() > Capacity) {
                throw std::out_of_range("InlineString: capacity exceeded on string_view append");
            }
            for (std::size_t i = 0; i < sv.size(); ++i)
                data_[size_ + i] = sv[i];
            size_ += sv.size();
            return *this;
        }

        /// @throw std::out_of_range if capacity is exceeded
        constexpr InlineString& operator+=(const char c) {
            if (size_ >= Capacity) {
                throw std::out_of_range("InlineString: capacity exceeded on char append");
            }
            data_[size_++] = c;
            return *this;
        }

        /// Replace contents with @p sv. At runtime, truncates silently if
        /// @p sv exceeds Capacity. At consteval, throws on overflow - which is
        /// a compile error, providing compile-time size checking for literal
        /// sources.
        constexpr InlineString& assign(std::string_view sv) {
            if consteval {
                if (sv.size() > Capacity) {
                    throw std::out_of_range("InlineString: assign exceeds capacity");
                }
            }
            const std::size_t n = std::min(sv.size(), Capacity);
            for (std::size_t i = 0; i < n; ++i) {
                data_[i] = sv[i];
            }
            data_[n] = '\0';
            size_    = n;
            return *this;
        }

        /// Resets to an empty string.
        constexpr void clear() noexcept {
            size_    = 0;
            data_[0] = '\0';
        }

        /// Returns a `string_view` over the current contents.
        [[nodiscard]] constexpr std::string_view view() const noexcept {
            return {data_, size_};
        }

        /// Explicit conversion to `string_view`; equivalent to `view()`.
        [[nodiscard]] constexpr explicit operator std::string_view() const noexcept {
            return view();
        }

        /// Returns the current length in bytes.
        [[nodiscard]] constexpr std::size_t size() const noexcept {
            return size_;
        }

        /// True iff `size() == 0`.
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        /// Returns a pointer to the null-terminated character storage.
        [[nodiscard]] constexpr const char* data() const noexcept {
            return data_;
        }

        /// Returns a null-terminated C string pointing at the stored characters.
        [[nodiscard]] constexpr const char* c_str() const noexcept {
            return data_;
        }

        /// True iff the contents equal `other`.
        constexpr bool operator==(const std::string_view other) const noexcept {
            return view() == other;
        }
    };
}  // namespace menagerie::beavers
