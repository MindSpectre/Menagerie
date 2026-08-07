#pragma once

#include <charconv>
#include <cstring>
#include <menagerie/beavers>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <variant>

#include <db_exceptions.hpp>
#include <db_field_value.hpp>
#include <libpq-fe.h>
#include <postgres_format_registry.hpp>

namespace menagerie::db::postgres {
    /// Non-owning view over a single result column value; decodes on demand
    /// into any FieldValue-representable C++ type via `as<T>()`/`get<T>()`.
    class FieldView {
    public:
        /**
         * @brief Constructs a view over one column value. Does not copy or take
         *        ownership of `ptr`; it must outlive this FieldView.
         * @param ptr Pointer to the raw value bytes (as returned by PQgetvalue()),
         *        or nullptr when is_null is true.
         * @param len Length of the value in bytes (as returned by PQgetlength()).
         * @param is_null Whether the column value is SQL NULL.
         * @param format Wire format the value is encoded in (FormatRegistry::text
         *        or FormatRegistry::binary).
         * @param oid PostgreSQL type OID of the column (pg_type.oid), used to pick
         *        the correct binary-format decoder.
         */
        FieldView(
            const char* const ptr, const std::size_t len, const bool is_null, const std::uint32_t format, const Oid oid)
            : ptr_{ptr},
              len_{len},
              is_null_{is_null},
              format_{format},
              oid_{oid} {
        }

        /// Whether the underlying value is SQL NULL.
        [[nodiscard]] bool is_null() const noexcept {
            return is_null_;
        }

        /// The raw value bytes as a string_view. Valid only while the owning
        /// PGresult (and this FieldView) lives.
        [[nodiscard]] std::string_view as_sv() const noexcept {
            return {ptr_, static_cast<size_t>(len_)};
        }

        /**
         * @brief Decodes the value as T, or std::nullopt if it is SQL NULL.
         * @throw std::runtime_error if the value is present but fails to decode
         *        as T (e.g. malformed numeric/float text, or an unsupported
         *        bytea format).
         */
        template <typename T>
        [[nodiscard]] std::optional<T> get() const {
            if (is_null_)
                return std::nullopt;
            return as<T>();
        }

        /**
         * @brief Decodes the value as T, dispatching to the type-specific
         *        decode_*() implementation for T at compile time.
         * @throw null_conversion_error if the value is SQL NULL and T is not
         *        std::monostate.
         * @throw std::runtime_error if the value is present but fails to decode
         *        as T (e.g. malformed numeric/float text, or an unsupported
         *        bytea format).
         */
        template <typename T>
        [[nodiscard]] T as() const {
            if (is_null_ && !std::is_same_v<T, std::monostate>) {
                //? do we really need exception here
                null_conversion_error err;
                err << column_info("unknown") << sqlstate_info("22002");
                boost::throw_with_location(std::move(err));  // null_value_not_allowed
            }
            if constexpr (std::is_same_v<T, std::monostate>) {
                return std::monostate{};
            } else if constexpr (std::is_same_v<T, bool>) {
                return decode_bool();
            } else if constexpr (std::is_same_v<T, char>) {
                return decode_char();
            } else if constexpr (std::is_same_v<T, int16_t>) {
                return decode_int16();
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return decode_int32();
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return decode_int64();
            } else if constexpr (std::is_same_v<T, uint16_t>) {
                return decode_uint16();
            } else if constexpr (std::is_same_v<T, uint32_t>) {
                return decode_uint32();
            } else if constexpr (std::is_same_v<T, uint64_t>) {
                return decode_uint64();
            } else if constexpr (std::is_same_v<T, float>) {
                return decode_float();
            } else if constexpr (std::is_same_v<T, double>) {
                return decode_double();
            } else if constexpr (std::is_same_v<T, std::string>) {
                return decode_string();
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                return decode_string_view();
            } else if constexpr (std::is_same_v<T, std::span<const std::uint8_t>>) {
                return decode_binary_span();
            } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
                return decode_binary_vector();
            } else {
                static_assert(menagerie::beavers::dependent_false_v<T>,
                              "Unsupported type for FieldView::as<T>(). "
                              "Supported types: bool, char, int16_t, int32_t, int64_t, "
                              "uint16_t, uint32_t, uint64_t, float, double, string, "
                              "string_view, span<const uint8_t>");
                std::unreachable();
            }
        }

    private:
        // -------- Text decoders (fast path via from_chars) --------
        template <typename T>
        [[nodiscard]] T decode_integer_text() const {
            T result{};
            auto sv        = as_sv();
            auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result);
            if (ec != std::errc{}) {
                throw std::runtime_error{"Failed to parse integer from: " + std::string(sv)};
            }
            return result;
        }
        // -------- Individual type decoders --------
        [[nodiscard]] bool decode_bool() const;

        [[nodiscard]] char decode_char() const;

        [[nodiscard]] int16_t decode_int16() const;

        [[nodiscard]] int32_t decode_int32() const;

        [[nodiscard]] int64_t decode_int64() const;

        [[nodiscard]] uint16_t decode_uint16() const;

        [[nodiscard]] uint32_t decode_uint32() const;

        [[nodiscard]] uint64_t decode_uint64() const;

        [[nodiscard]] float decode_float() const;

        [[nodiscard]] double decode_double() const;

        [[nodiscard]] std::string decode_string() const;

        [[nodiscard]] std::string_view decode_string_view() const;

        [[nodiscard]] std::span<const std::uint8_t> decode_binary_span() const;

        [[nodiscard]] std::vector<std::uint8_t> decode_binary_vector() const;

        [[nodiscard]] std::vector<std::uint8_t> decode_hex_bytea() const;

        const char* ptr_      = nullptr;
        std::size_t len_      = 0;
        bool is_null_         = true;
        std::uint32_t format_ = FormatRegistry::text;  ///< 0=text, 1=binary
        Oid oid_              = 0;
    };


    // Compile-time check that we handle all FieldValue types


    /// Non-owning view over one row of a PGresult, providing column access by
    /// index or by name. Valid only while the owning PGresult lives.
    class RowView {
    public:
        /// Constructs a view over row `row` of `r`. Does not copy or take
        /// ownership of `r`.
        RowView(PGresult* r, const std::size_t row)
            : res_{r},
              row_{row} {
        }

        /// Number of columns in the row (same as the result's column count).
        [[nodiscard]] std::size_t cols() const noexcept {
            return static_cast<std::size_t>(PQnfields(res_));
        }

        /// Returns a view onto column `col` of this row.
        [[nodiscard]] FieldView at(const std::size_t col) const {
            const bool is_null = PQgetisnull(res_, static_cast<int>(row_), static_cast<int>(col));
            const auto f       = static_cast<std::uint32_t>(PQfformat(res_, static_cast<int>(col)));
            const Oid oid      = PQftype(res_, static_cast<int>(col));
            const char* p      = is_null ? nullptr : PQgetvalue(res_, static_cast<int>(row_), static_cast<int>(col));
            const std::size_t len =
                is_null ? 0
                        : static_cast<std::size_t>(PQgetlength(res_, static_cast<int>(row_), static_cast<int>(col)));
            return FieldView{p, len, is_null, f, oid};
        }

        /// Equivalent to at(col).
        [[nodiscard]] FieldView operator[](const std::size_t col) const {
            return at(col);
        }

        /**
         * @brief Looks up the index of the column named `name`.
         * @throw std::out_of_range if no column with that name exists.
         */
        [[nodiscard]] std::size_t col_index(const std::string_view name) const {
            const int idx = PQfnumber(res_, std::string{name}.c_str());
            if (idx == -1)
                throw std::out_of_range{"Column is not found"};
            return static_cast<std::size_t>(idx);
        }

    private:
        PGresult* res_   = nullptr;
        std::size_t row_ = 0;
    };
}  // namespace menagerie::db::postgres
