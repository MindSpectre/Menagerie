#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace menagerie::db::postgres {

    /// Compile-time helpers that build fixed-form "PREFIX(N)" and
    /// "NUMERIC(Precision,Scale)" SQL type-name strings for SqlTypeRegistry's
    /// parameterized type functions.
    namespace detail {

        /// Number of decimal digits needed to print N (1-5 digits; N must be <= 99999).
        template <std::size_t N>
        constexpr std::size_t digit_count() {
            if constexpr (N < 10)
                return 1;
            else if constexpr (N < 100)
                return 2;
            else if constexpr (N < 1000)
                return 3;
            else if constexpr (N < 10000)
                return 4;
            else
                return 5;
        }

        /// Compile-time storage for a "PREFIX(N)" string, built from a prefix
        /// character pack and a single numeric parameter N (N <= 99999).
        template <std::size_t PrefixLen, std::size_t N, char... Prefix>
        struct SingleParamStorage {
            static_assert(N <= 99999, "Parameter value too large");

            static constexpr std::size_t length = PrefixLen + 1 + digit_count<N>() + 1;  ///< PREFIX + ( + digits + )

            /// The null-padded "PREFIX(N)" characters; only the first `length` bytes are meaningful.
            static constexpr auto value = []() {
                std::array<char, 32> result{};
                std::size_t pos = 0;

                for (const char c : {Prefix...}) {
                    result[pos++] = c;
                }
                result[pos++] = '(';

                // Write digits
                if constexpr (N >= 10000)
                    result[pos++] = static_cast<char>('0' + (N / 10000) % 10);
                if constexpr (N >= 1000)
                    result[pos++] = static_cast<char>('0' + (N / 1000) % 10);
                if constexpr (N >= 100)
                    result[pos++] = static_cast<char>('0' + (N / 100) % 10);
                if constexpr (N >= 10)
                    result[pos++] = static_cast<char>('0' + (N / 10) % 10);
                result[pos++] = static_cast<char>('0' + N % 10);

                result[pos++] = ')';
                return result;
            }();
        };

        /// Compile-time "CHAR(N)" string.
        template <std::size_t N>
        using CharStorage = SingleParamStorage<4, N, 'C', 'H', 'A', 'R'>;

        /// Compile-time "VARCHAR(N)" string.
        template <std::size_t N>
        using VarcharStorage = SingleParamStorage<7, N, 'V', 'A', 'R', 'C', 'H', 'A', 'R'>;

        /// Compile-time "BIT(N)" string.
        template <std::size_t N>
        using BitStorage = SingleParamStorage<3, N, 'B', 'I', 'T'>;

        /// Compile-time "VARBIT(N)" string.
        template <std::size_t N>
        using VarbitStorage = SingleParamStorage<6, N, 'V', 'A', 'R', 'B', 'I', 'T'>;

        /// Compile-time "TIME(P)" string.
        template <std::size_t P>
        using TimeStorage = SingleParamStorage<4, P, 'T', 'I', 'M', 'E'>;

        /// Compile-time "TIMESTAMP(P)" string.
        template <std::size_t P>
        using TimestampStorage = SingleParamStorage<9, P, 'T', 'I', 'M', 'E', 'S', 'T', 'A', 'M', 'P'>;

        /// Compile-time "TIMESTAMP(P) WITH TIME ZONE" string. P must be 0-6;
        /// the fixed-size output buffer assumes a single decimal digit for P.
        template <std::size_t P>
        struct TimestampTzStorage {
            static_assert(P <= 6, "TIMESTAMPTZ precision must be 0-6");
            static constexpr std::size_t length         = 30;  ///< "TIMESTAMP(P) WITH TIME ZONE" length.
            /// The null-padded "TIMESTAMP(P) WITH TIME ZONE" characters, `length` bytes meaningful.
            static constexpr std::array<char, 32> value = {
                'T', 'I', 'M', 'E', 'S', 'T', 'A', 'M', 'P', '(', static_cast<char>('0' + P),
                ')', ' ', 'W', 'I', 'T', 'H', ' ', 'T', 'I', 'M', 'E',
                ' ', 'Z', 'O', 'N', 'E'};
        };

        /// Compile-time "INTERVAL(P)" string.
        template <std::size_t P>
        using IntervalStorage = SingleParamStorage<8, P, 'I', 'N', 'T', 'E', 'R', 'V', 'A', 'L'>;

        /// Compile-time "NUMERIC(Precision,Scale)" string. Precision and Scale
        /// are limited to 0-99 (2 digits each).
        template <std::size_t Precision, std::size_t Scale>
        struct NumericStorage {
            static_assert(Precision <= 99 && Scale <= 99, "NUMERIC supports up to 2 digits for precision/scale");

            static constexpr std::size_t length = 8 + digit_count<Precision>() + 1 + digit_count<Scale>() + 1;  ///< "NUMERIC(Precision,Scale)" length.

            /// The null-padded "NUMERIC(Precision,Scale)" characters; only the first `length` bytes are meaningful.
            static constexpr auto value = []() {
                std::array<char, 16> result{};
                std::size_t pos = 0;

                for (const char c : {'N', 'U', 'M', 'E', 'R', 'I', 'C', '('}) {
                    result[pos++] = c;
                }

                if constexpr (Precision >= 10) {
                    result[pos++] = static_cast<char>('0' + (Precision / 10));
                }
                result[pos++] = static_cast<char>('0' + (Precision % 10));

                result[pos++] = ',';

                if constexpr (Scale >= 10) {
                    result[pos++] = static_cast<char>('0' + (Scale / 10));
                }
                result[pos++] = static_cast<char>('0' + (Scale % 10));

                result[pos++] = ')';

                return result;
            }();
        };

    }  // namespace detail

    /**
     * @brief PostgreSQL SQL type-name strings, used to render column type
     *        declarations for the C++ types mapped in postgres_type_mapping.hpp.
     *
     * Fixed-form types (SMALLINT, TEXT, BOOLEAN, ...) are plain static
     * std::string_view constants; parameterized types (CHAR(N), VARCHAR(N),
     * NUMERIC(P,S), ...) are static template functions that build their SQL
     * string at compile time via the detail:: storage helpers above.
     */
    struct SqlTypeRegistry {
        // -------- Numeric types --------

        constexpr static std::string_view smallint         = "SMALLINT";           ///< SMALLINT type name.
        constexpr static std::string_view integer          = "INTEGER";            ///< INTEGER type name.
        constexpr static std::string_view bigint           = "BIGINT";             ///< BIGINT type name.
        constexpr static std::string_view real             = "REAL";               ///< REAL type name.
        constexpr static std::string_view double_precision = "DOUBLE PRECISION";   ///< DOUBLE PRECISION type name.
        constexpr static std::string_view smallserial      = "SMALLSERIAL";        ///< SMALLSERIAL type name.
        constexpr static std::string_view serial           = "SERIAL";             ///< SERIAL type name.
        constexpr static std::string_view bigserial        = "BIGSERIAL";          ///< BIGSERIAL type name.
        constexpr static std::string_view money            = "MONEY";              ///< MONEY type name.


        // -------- Character types --------

        constexpr static std::string_view text = "TEXT";  ///< TEXT type name.


        // -------- Binary types --------

        constexpr static std::string_view bytea = "BYTEA";  ///< BYTEA type name.


        // -------- Boolean type --------

        constexpr static std::string_view boolean = "BOOLEAN";  ///< BOOLEAN type name.


        // -------- Date/Time types --------

        constexpr static std::string_view date        = "DATE";                     ///< DATE type name.
        constexpr static std::string_view time        = "TIME";                     ///< TIME type name.
        constexpr static std::string_view timetz      = "TIME WITH TIME ZONE";       ///< TIME WITH TIME ZONE type name.
        constexpr static std::string_view timestamp   = "TIMESTAMP";                 ///< TIMESTAMP type name.
        constexpr static std::string_view timestamptz = "TIMESTAMP WITH TIME ZONE";  ///< TIMESTAMP WITH TIME ZONE type name.
        constexpr static std::string_view interval    = "INTERVAL";                  ///< INTERVAL type name.


        // -------- UUID type --------

        constexpr static std::string_view uuid = "UUID";  ///< UUID type name.


        // -------- JSON types --------

        constexpr static std::string_view json  = "JSON";   ///< JSON type name.
        constexpr static std::string_view jsonb = "JSONB";  ///< JSONB type name.


        // -------- Network types --------

        constexpr static std::string_view inet    = "INET";     ///< INET type name.
        constexpr static std::string_view cidr    = "CIDR";     ///< CIDR type name.
        constexpr static std::string_view macaddr = "MACADDR";  ///< MACADDR type name.


        // -------- Other types --------

        constexpr static std::string_view xml = "XML";  ///< XML type name.
        constexpr static std::string_view oid = "OID";  ///< OID type name.


        // -------- Parameterized types (compile-time generation) --------


        /// CHAR(N) - fixed-length character type name.
        template <std::size_t N>
        static constexpr std::string_view char_type() {
            return {detail::CharStorage<N>::value.data(), detail::CharStorage<N>::length};
        }

        /// VARCHAR(N) - variable-length character type name with a length limit.
        template <std::size_t N>
        static constexpr std::string_view varchar() {
            return {detail::VarcharStorage<N>::value.data(), detail::VarcharStorage<N>::length};
        }

        /// NUMERIC(Precision,Scale) - exact numeric type name.
        template <std::size_t Precision, std::size_t Scale>
        static constexpr std::string_view numeric() {
            return {detail::NumericStorage<Precision, Scale>::value.data(),
                    detail::NumericStorage<Precision, Scale>::length};
        }

        /// BIT(N) - fixed-length bit string type name.
        template <std::size_t N>
        static constexpr std::string_view bit() {
            return {detail::BitStorage<N>::value.data(), detail::BitStorage<N>::length};
        }

        /// VARBIT(N) - variable-length bit string type name.
        template <std::size_t N>
        static constexpr std::string_view varbit() {
            return {detail::VarbitStorage<N>::value.data(), detail::VarbitStorage<N>::length};
        }

        /// TIME(P) - time-of-day type name with P digits of fractional-second precision (0-6).
        template <std::size_t P>
        static constexpr std::string_view time_p() {
            static_assert(P <= 6, "TIME precision must be 0-6");
            return {detail::TimeStorage<P>::value.data(), detail::TimeStorage<P>::length};
        }

        /// TIMESTAMP(P) - timestamp type name with P digits of fractional-second precision (0-6).
        template <std::size_t P>
        static constexpr std::string_view timestamp_p() {
            static_assert(P <= 6, "TIMESTAMP precision must be 0-6");
            return {detail::TimestampStorage<P>::value.data(), detail::TimestampStorage<P>::length};
        }

        /// TIMESTAMP(P) WITH TIME ZONE - timestamptz type name with P digits of
        /// fractional-second precision.
        template <std::size_t P>
        static constexpr std::string_view timestamptz_p() {
            return {detail::TimestampTzStorage<P>::value.data(), detail::TimestampTzStorage<P>::length};
        }
    };

}  // namespace menagerie::db::postgres
