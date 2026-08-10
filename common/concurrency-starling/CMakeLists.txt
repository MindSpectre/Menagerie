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
add_library(${STARLING}.ThreadSafeResource INTERFACE
        thread_safe_resource/thread_safe_resource.hpp
)
target_include_directories(${STARLING}.ThreadSafeResource
        INTERFACE
        thread_safe_resource/
)
target_link_libraries(${STARLING}.ThreadSafeResource
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# Thread pool for paralleling tasks
##############################################################################
add_library(${STARLING}.ThreadPool STATIC
        thread_pool/thread_pool.cpp
        thread_pool/thread_pool.hpp
        thread_pool/detail/enqueued_task.hpp
        thread_pool/config/thread_pool_config.hpp
)
target_include_directories(${STARLING}.ThreadPool PUBLIC
        thread_pool/

)

target_link_libraries(${STARLING}.ThreadPool PUBLIC
        Menagerie::Common::Beaver
        ${STARLING}.ThreadSafeResource
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
        ${STARLING}.ThreadPool
        ${STARLING}.ThreadSafeResource
        ${STARLING}.Disruptor
        ${STARLING}.EventCount
        ${STARLING}.ResourcePool.Sync
        ${STARLING}.ResourcePool.Async
        ${STARLING}.AsioBackend
)

add_library(Menagerie::Common::Starling ALIAS ${STARLING})
##############################################################################
