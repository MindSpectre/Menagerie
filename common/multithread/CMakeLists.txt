set(MULTITHREAD ${COMMON}.Multithread)

##############################################################################
# Multithread utils
##############################################################################
add_library(${MULTITHREAD}.Utils INTERFACE
        utils/pause.hpp
        utils/thread_pinning.hpp
)
target_include_directories(${MULTITHREAD}.Utils
        INTERFACE
        utils/
)
target_link_libraries(${MULTITHREAD}.Utils
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# EventCount - folly-style futex wait primitive
##############################################################################
add_library(${MULTITHREAD}.EventCount INTERFACE
        event_count/event_count.hpp
)
target_include_directories(${MULTITHREAD}.EventCount
        INTERFACE
        event_count/
)
target_link_libraries(${MULTITHREAD}.EventCount
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${MULTITHREAD}.ResourcePool.Sync INTERFACE
        resource_pool/sync/resource_pool.hpp
        resource_pool/sync/detail/lease.hpp
)
target_include_directories(${MULTITHREAD}.ResourcePool.Sync
        INTERFACE
        resource_pool/sync
)
target_link_libraries(${MULTITHREAD}.ResourcePool.Sync
        INTERFACE
        Threads::Threads
        Menagerie::Common::Beavers
        ${MULTITHREAD}.EventCount
        ${MULTITHREAD}.Utils
)
##############################################################################

##############################################################################
# ResourcePool - partition-aware bounded resource pool
##############################################################################
add_library(${MULTITHREAD}.ResourcePool.Async INTERFACE
        resource_pool/async/async_resource_pool.hpp
        resource_pool/async/detail/async_lease.hpp
)
target_include_directories(${MULTITHREAD}.ResourcePool.Async
        INTERFACE
        resource_pool/async
)
target_link_libraries(${MULTITHREAD}.ResourcePool.Async
        INTERFACE
        Threads::Threads
        Boost::asio
        Boost::system
        Menagerie::Common::Beavers
        ${MULTITHREAD}.EventCount
        ${MULTITHREAD}.Utils
)
##############################################################################

##############################################################################
# AsioBackend - RAII io_context runners (single + sharded) for asio-based pools
##############################################################################
add_library(${MULTITHREAD}.AsioBackend INTERFACE
        asio_backend/asio_backend.hpp
        asio_backend/sharded_asio_backend.hpp
)
target_include_directories(${MULTITHREAD}.AsioBackend
        INTERFACE
        asio_backend
)
target_link_libraries(${MULTITHREAD}.AsioBackend
        INTERFACE
        Threads::Threads
        Boost::asio
        Boost::system
        ${MULTITHREAD}.Utils
)
##############################################################################

##############################################################################
# Synchronized resource class - wraps resource with mutex
##############################################################################
add_library(${MULTITHREAD}.ThreadSafeResource INTERFACE
        thread_safe_resource/thread_safe_resource.hpp
)
target_include_directories(${MULTITHREAD}.ThreadSafeResource
        INTERFACE
        thread_safe_resource/
)
target_link_libraries(${MULTITHREAD}.ThreadSafeResource
        INTERFACE
        Threads::Threads
)
##############################################################################


##############################################################################
# Thread pool for paralleling tasks
##############################################################################
add_library(${MULTITHREAD}.ThreadPool STATIC
        thread_pool/thread_pool.cpp
        thread_pool/thread_pool.hpp
        thread_pool/detail/enqueued_task.hpp
        thread_pool/config/thread_pool_config.hpp
)
target_include_directories(${MULTITHREAD}.ThreadPool PUBLIC
        thread_pool/

)

target_link_libraries(${MULTITHREAD}.ThreadPool PUBLIC
        Menagerie::Common::Beavers
        ${MULTITHREAD}.ThreadSafeResource
)
##############################################################################


##############################################################################
# Disruptor - Lock-free ring buffer for high-performance messaging
##############################################################################
add_subdirectory(disruptor)
##############################################################################


##############################################################################
# Multithread exported library
##############################################################################
add_combined_library(${MULTITHREAD}
        SOURCES
        export/menagerie/multithread
        DIRECTORIES
        export
        LIBRARIES
        ${MULTITHREAD}.ThreadPool
        ${MULTITHREAD}.ThreadSafeResource
        ${MULTITHREAD}.Disruptor
        ${MULTITHREAD}.EventCount
        ${MULTITHREAD}.ResourcePool.Sync
        ${MULTITHREAD}.ResourcePool.Async
        ${MULTITHREAD}.AsioBackend
)

add_library(Menagerie::Common::Multithread ALIAS ${MULTITHREAD})
##############################################################################
