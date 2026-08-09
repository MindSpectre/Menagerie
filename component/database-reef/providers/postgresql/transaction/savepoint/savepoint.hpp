#pragma once

#include <menagerie/beaver>
#include <menagerie/crow>
#include <string>
#include <string_view>

#include <postgres_errors.hpp>
namespace menagerie::reef::postgres {

    /**
     * @brief Scoped `SAVEPOINT` within an active Transaction.
     *
     * Wraps `SAVEPOINT` / `RELEASE SAVEPOINT` / `ROLLBACK TO SAVEPOINT` around a raw
     * PGconn* borrowed from the owning Transaction, since a savepoint never outlives
     * the transaction that created it. Move-only; if still active at destruction the
     * savepoint is released (not rolled back).
     */
    class Savepoint : beaver::NonCopyable {
    public:
        /// Releases the savepoint if it is still active.
        ~Savepoint();

        /// Transfers ownership of the underlying savepoint; other becomes inactive.
        Savepoint(Savepoint&& other) noexcept;
        /// Releases this savepoint if active, then takes over other's; other becomes inactive.
        Savepoint& operator=(Savepoint&& other) noexcept;

        /// Rolls back to this savepoint, undoing work done since it was created; stays active on success.
        [[nodiscard]] beaver::Outcome<void, ErrorContext> rollback();
        /// Releases this savepoint, folding its work into the enclosing transaction; becomes inactive on success.
        [[nodiscard]] beaver::Outcome<void, ErrorContext> release();

        /// Name this savepoint was created with.
        [[nodiscard]] constexpr std::string_view name() const noexcept {
            return name_;
        }

        /// True until release() succeeds or the savepoint is destroyed.
        [[nodiscard]] constexpr bool is_active() const noexcept {
            return active_;
        }

    private:
        friend class Transaction;

        CROW_COMPONENT_PREFIX("Savepoint");

        PGconn* conn_;
        std::string name_;
        bool active_ = true;

        template <beaver::IsStringLike StringTp>
        constexpr Savepoint(PGconn* conn, StringTp&& name)
            : conn_{conn},
              name_{std::forward<StringTp>(name)} {
            COMPONENT_LOG_INF() << "Savepoint '" << name_ << "' created";
        }

        [[nodiscard]] beaver::Outcome<void, ErrorContext> execute_control(const std::string& sql) const;
    };

}  // namespace menagerie::reef::postgres
