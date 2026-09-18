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
# Pool - passive fixed-arena resource pool
##############################################################################
add_library(${STARLING}.Pool INTERFACE
        pool/pool.hpp
        pool/types.hpp
        pool/detail/deadline.hpp
        pool/detail/lifetime.hpp
        pool/detail/waiter.hpp
        pool/detail/borrowed.hpp
        pool/detail/reserved_slot.hpp
        pool/detail/sync_acquire.hpp
        pool/detail/async_acquire.hpp
)
target_include_directories(${STARLING}.Pool
        INTERFACE
        pool
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
        ${STARLING}.Pool
        ${STARLING}.AsioBackend
)

add_library(Menagerie::Common::Starling ALIAS ${STARLING})
##############################################################################
