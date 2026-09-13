set(STARLING ${COMMON}.Starling)

##############################################################################
# Starling utils
##############################################################################
add_library(${STARLING}.Utils INTERFACE
        utils/pause.hpp
        utils/thread_pinning.hpp
)
target_include_directories(${STARLING}.Utils
        INTERFACE
        utils/
)
target_link_libraries(${STARLING}.Utils
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# EventCount - folly-style futex wait primitive
##############################################################################
add_library(${STARLING}.EventCount INTERFACE
        event_count/event_count.hpp
)
target_include_directories(${STARLING}.EventCount
        INTERFACE
        event_count/
)
target_link_libraries(${STARLING}.EventCount
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${STARLING}.ResourcePool.Sync INTERFACE
        resource_pool/sync/resource_pool.hpp
        resource_pool/sync/detail/lease.hpp
)
target_include_directories(${STARLING}.ResourcePool.Sync
        INTERFACE
        resource_pool/sync
)
target_link_libraries(${STARLING}.ResourcePool.Sync
        INTERFACE
        Threads::Threads
        Menagerie::Common::Beaver
        ${STARLING}.EventCount
        ${STARLING}.Utils
)
##############################################################################

##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${STARLING}.ResourcePool.Async INTERFACE
        resource_pool/async/async_resource_pool.hpp
        resource_pool/async/detail/async_lease.hpp
)
target_include_directories(${STARLING}.ResourcePool.Async
        INTERFACE
        resource_pool/async
)
target_link_libraries(${STARLING}.ResourcePool.Async
        INTERFACE
        Threads::Threads
        Boost::asio
        Boost::system
        Menagerie::Common::Beaver
        ${STARLING}.EventCount
        ${STARLING}.Utils
)
##############################################################################

##############################################################################
# Pool - passive fixed-arena resource pool
##############################################################################
add_library(${STARLING}.Pool INTERFACE
        resource_pool/pool/pool.hpp
        resource_pool/pool/types.hpp
        resource_pool/pool/detail/deadline.hpp
        resource_pool/pool/detail/lifetime.hpp
        resource_pool/pool/detail/waiter.hpp
        resource_pool/pool/detail/borrowed.hpp
        resource_pool/pool/detail/reserved_slot.hpp
        resource_pool/pool/detail/sync_acquire.hpp
        resource_pool/pool/detail/async_acquire.hpp
)
target_include_directories(${STARLING}.Pool
        INTERFACE
        resource_pool/pool
)
target_link_libraries(${STARLING}.Pool
        INTERFACE
        Threads::Threads
        Boost::asio
        Boost::system
        Menagerie::Common::Beaver
)
##############################################################################

##############################################################################
# AsioBackend - RAII io_context runners (single + sharded) for asio-based pools
##############################################################################
add_library(${STARLING}.AsioBackend INTERFACE
        asio_backend/asio_backend.hpp
        asio_backend/sharded_asio_backend.hpp
)
target_include_directories(${STARLING}.AsioBackend
        INTERFACE
        asio_backend
)
target_link_libraries(${STARLING}.AsioBackend
        INTERFACE
        Threads::Threads
        Boost::asio
        Boost::system
        ${STARLING}.Utils
)
##############################################################################

##############################################################################
# Synchronized resource class - wraps resource with mutex
##############################################################################
add_library(${STARLING}.SynchronizedResource INTERFACE
        synchronized_resource/synchronized_resource.hpp
)
target_include_directories(${STARLING}.SynchronizedResource
        INTERFACE
        synchronized_resource/
)
target_link_libraries(${STARLING}.SynchronizedResource
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# Shared resource class - wraps resource with a reader-writer mutex
##############################################################################
add_library(${STARLING}.SharedResource INTERFACE
        shared_resource/shared_resource.hpp
)
target_include_directories(${STARLING}.SharedResource
        INTERFACE
        shared_resource/
)
target_link_libraries(${STARLING}.SharedResource
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# Disruptor - Lock-free ring buffer for high-performance messaging
##############################################################################
add_subdirectory(disruptor)
##############################################################################


##############################################################################
# Starling exported library
##############################################################################
add_combined_library(${STARLING}
        SOURCES
        export/menagerie/starling
        DIRECTORIES
        export
        LIBRARIES
        ${STARLING}.SynchronizedResource
        ${STARLING}.SharedResource
        ${STARLING}.Disruptor
        ${STARLING}.EventCount
        ${STARLING}.ResourcePool.Sync
        ${STARLING}.ResourcePool.Async
        ${STARLING}.Pool
        ${STARLING}.AsioBackend
)

add_library(Menagerie::Common::Starling ALIAS ${STARLING})
##############################################################################
