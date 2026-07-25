#pragma once
#include <cstring>
#include <deque>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include <postgres_oid_type_registry.hpp>
#include <sql_params.hpp>

namespace menagerie::db::postgres {

    /**
     * @brief Backing storage for a bound PQexecParams()-style call: parallel per-parameter
     *        arrays plus the string/byte storage the value pointers point into.
     */
    struct Params {
        std::pmr::vector<const char*> values;  ///< Per-parameter value pointer; nullptr means SQL NULL.
        std::pmr::vector<int> lengths;         ///< Per-parameter byte length (ignored for text values).
        std::pmr::vector<int> formats;         ///< Per-parameter format: 0 text, 1 binary.
        std::pmr::vector<unsigned> oids;       ///< Per-parameter PostgreSQL type OID; 0 lets the server infer it.
        std::pmr::deque<std::pmr::string> str_data;                  ///< Backing storage for text-format values (deque for pointer stability).
        std::pmr::deque<std::pmr::vector<std::byte>> binary_chunks;  ///< Backing storage for binary-format values, one vector per parameter.

        /// Debug-prints every bound parameter: index, format, OID, and value (hex for binary).
        friend std::ostream& operator<<(std::ostream& os, const Params& p) {
            os << "Params[" << p.values.size() << "]";
            if (p.values.empty())
                return os;
            os << ":\n";
            for (std::size_t i = 0; i < p.values.size(); ++i) {
                os << "  $" << (i + 1) << ": ";
                if (p.values[i] == nullptr) {
                    os << "NULL";
                } else if (p.formats[i] == 0) {
                    os << "text  oid=" << p.oids[i] << " val=\"" << p.values[i] << '"';
                } else {
                    os << "binary oid=" << p.oids[i] << " len=" << p.lengths[i] << " val=0x";
                    for (int j = 0; j < p.lengths[i]; ++j)
                        os << std::format("{:02x}", static_cast<unsigned char>(p.values[i][j]));
                }
                os << '\n';
            }
            return os;
        }
    };

    /**
     * @brief PostgreSQL implementation of db::ParamSink.
     *
     * Binds FieldValue arguments to libpq $N parameters, encoding fixed-width numeric
     * types in network byte order and falling back to text format (via native_packet()'s
     * Params::str_data) for values libpq has no binary form for, such as unsigned 64-bit
     * integers bound as NUMERIC.
     */
    class ParamSink final : public db::ParamSink {
    public:
        /// Binds all backing storage in the sink's Params packet to `mr`.
        explicit ParamSink(std::pmr::memory_resource* mr)
            : mr_{mr},
              params_{std::make_shared<Params>(
                  Params{.values{mr}, .lengths{mr}, .formats{mr}, .oids{mr}, .str_data{mr}, .binary_chunks{mr}})} {
        }

        /// Copies v into the sink, returning its 1-based placeholder index.
        std::size_t push(const FieldValue& v) override {
            std::visit([this](const auto& x) { bind_one(x); }, v);
            return params_->values.size();  // 1-based index for PostgreSQL
        }

        /// @warning No true move. Must be copied
        std::size_t push(FieldValue&& v) override {
            return push(v);
        }

        /// @brief Expose packet via shared_ptr<void>
        [[nodiscard]] std::shared_ptr<void> packet() const noexcept override {
            return params_;
        }
        /// @brief Expose packet in native format
        [[nodiscard]] std::shared_ptr<Params> native_packet() const noexcept {
            return params_;
        }

        // TODO: think about adding binary format flag(indicate dont use binary format at all)
    private:
        // Specializations for each type
        void bind_one(std::monostate) const;

        void bind_one(bool b) const;

        void bind_one(char c) const;

        void bind_one(std::int16_t i) const;

        void bind_one(std::int32_t i) const;

        void bind_one(std::int64_t i) const;

        void bind_one(std::uint16_t i) const;

        void bind_one(std::uint32_t i) const;

        void bind_one(std::uint64_t i) const;

        void bind_one(float f) const;

        void bind_one(double d) const;

        void bind_one(const std::string& s) const;

        void bind_one(std::string_view s_view) const;

        void bind_one(const std::vector<std::uint8_t>& bytes) const;

        void bind_one(std::span<const std::uint8_t> bytes) const;

        std::pmr::memory_resource* mr_;
        std::shared_ptr<Params> params_;
    };

}  // namespace menagerie::db::postgres
