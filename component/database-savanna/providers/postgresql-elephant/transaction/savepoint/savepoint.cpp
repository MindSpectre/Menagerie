#include "savepoint.hpp"

#include <format>

#include <postgres_sync_executor.hpp>


namespace menagerie::savanna::elephant {

    Savepoint::~Savepoint() {
        if (active_) {
            std::ignore = release();
        }
    }

    Savepoint::Savepoint(Savepoint&& other) noexcept
        : conn_{std::exchange(other.conn_, nullptr)},
          name_{std::move(other.name_)},
          active_{std::exchange(other.active_, false)} {
    }

    Savepoint& Savepoint::operator=(Savepoint&& other) noexcept {
        if (this != &other) {
            if (active_) {
                std::ignore = release();
            }
            conn_   = std::exchange(other.conn_, nullptr);
            name_   = std::move(other.name_);
            active_ = std::exchange(other.active_, false);
        }
        return *this;
    }

    std::expected<void, ErrorContext> Savepoint::rollback() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control(std::format(R"(ROLLBACK TO SAVEPOINT "{}")", name_));
        return result;
    }

    std::expected<void, ErrorContext> Savepoint::release() {
        COMPONENT_LOG_ENTER_FUNCTION();
        if (!active_) {
            return std::unexpected(ErrorContext{ErrorCode{ClientErrorCode::InvalidState}});
        }
        auto result = execute_control(std::format(R"(RELEASE SAVEPOINT "{}")", name_));
        if (result.has_value()) {
            active_ = false;
        }
        return result;
    }

    std::expected<void, ErrorContext> Savepoint::execute_control(const std::string& sql) const {
        const SyncExecutor exec{conn_};
        if (auto result = exec.execute(sql); !result.has_value()) {
            return std::unexpected(std::move(result).error());
        }
        return {};
    }

}  // namespace menagerie::savanna::elephant
