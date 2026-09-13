#pragma once

#include <chrono>
#include <cstddef>
#include <expected>
#include <menagerie/starling>
#include <stdexcept>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>

#include "common/mock_resource.hpp"

namespace bench::pool {

    class PopulatedPool {
    public:
        using RawPool  = menagerie::starling::Pool<MockResource>;
        using Borrowed = typename RawPool::Borrowed;

        explicit PopulatedPool(const std::size_t capacity)
            : pool_{capacity} {
            for (std::size_t i = 0; i < capacity; ++i) {
                auto slot = pool_.try_reserve();
                if (!slot) {
                    throw std::logic_error{"benchmark failed to reserve initial slot"};
                }
                slot->emplace(i);
                if (!slot->publish()) {
                    throw std::logic_error{"benchmark failed to publish initial slot"};
                }
            }
        }

        auto try_acquire() {
            return pool_.try_acquire();
        }

        auto acquire_for(const std::chrono::nanoseconds timeout) {
            return pool_.acquire_for(timeout);
        }

        boost::asio::awaitable<std::expected<Borrowed, menagerie::starling::AcquireError>>
        async_acquire_for(boost::asio::any_io_executor, const std::chrono::nanoseconds timeout) {
            co_return co_await pool_.async_acquire_for(timeout);
        }

        RawPool& raw() noexcept {
            return pool_;
        }

    private:
        RawPool pool_;
    };

}  // namespace bench::pool
