#pragma once

#include <cstdint>

namespace menagerie::db::postgres {

    /// Well-known PostgreSQL type OIDs (pg_type.oid), used to pick the correct
    /// binary-format decoder when a query result column's wire format is
    /// FormatRegistry::binary.
    struct OidTypeRegistry {
        // -------- Numeric types --------
        constexpr static std::uint32_t oid_int2    = 21;    ///< int2 / smallint.
        constexpr static std::uint32_t oid_int4    = 23;    ///< int4 / integer.
        constexpr static std::uint32_t oid_int8    = 20;    ///< int8 / bigint.
        constexpr static std::uint32_t oid_float4  = 700;   ///< float4 / real.
        constexpr static std::uint32_t oid_float8  = 701;   ///< float8 / double precision.
        constexpr static std::uint32_t oid_numeric = 1700;  ///< numeric / decimal.
        constexpr static std::uint32_t oid_money   = 790;   ///< money.


        // -------- Character types --------
        constexpr static std::uint32_t oid_char    = 18;    ///< "char" (internal 1-byte type).
        constexpr static std::uint32_t oid_bpchar  = 1042;  ///< CHAR(n) / CHARACTER(n).
        constexpr static std::uint32_t oid_varchar = 1043;  ///< VARCHAR(n).
        constexpr static std::uint32_t oid_text    = 25;    ///< text.


        // -------- Binary types --------
        constexpr static std::uint32_t oid_bytea = 17;  ///< bytea (variable-length binary data).


        // -------- Boolean type --------
        constexpr static std::uint32_t oid_bool = 16;  ///< boolean.


        // -------- Date/Time types --------
        constexpr static std::uint32_t oid_date        = 1082;  ///< date.
        constexpr static std::uint32_t oid_time        = 1083;  ///< time without time zone.
        constexpr static std::uint32_t oid_timetz      = 1266;  ///< time with time zone.
        constexpr static std::uint32_t oid_timestamp   = 1114;  ///< timestamp without time zone.
        constexpr static std::uint32_t oid_timestamptz = 1184;  ///< timestamp with time zone.
        constexpr static std::uint32_t oid_interval    = 1186;  ///< interval.


        // -------- UUID type --------
        constexpr static std::uint32_t oid_uuid = 2950;  ///< uuid.


        // -------- JSON types --------
        constexpr static std::uint32_t oid_json  = 114;   ///< json (text-stored JSON).
        constexpr static std::uint32_t oid_jsonb = 3802;  ///< jsonb (binary-stored JSON).


        // -------- Network types --------
        constexpr static std::uint32_t oid_inet    = 869;  ///< inet (IP host/network address).
        constexpr static std::uint32_t oid_cidr    = 650;  ///< cidr (network address).
        constexpr static std::uint32_t oid_macaddr = 829;  ///< macaddr.


        // -------- Bit string types --------
        constexpr static std::uint32_t oid_bit    = 1560;  ///< bit(n), fixed-length bit string.
        constexpr static std::uint32_t oid_varbit = 1562;  ///< bit varying(n) / varbit.


        // -------- Other types --------
        constexpr static std::uint32_t oid_xml = 142;  ///< xml.
        constexpr static std::uint32_t oid_oid = 26;   ///< oid, the object identifier type itself.
    };

}  // namespace menagerie::db::postgres
