##############################################################################
# Http Listeners — ConnectionTracker (in-flight counting + force-cancel, D2)
##############################################################################
add_library(${Estuary}.Listeners.ConnectionTracker STATIC connection_tracker.cpp)

target_include_directories(${Estuary}.Listeners.ConnectionTracker PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${Estuary}.Listeners.ConnectionTracker
        PUBLIC
        ${Estuary}.Types.Executor
        Boost::asio
        Menagerie::Common::Beaver
)
##############################################################################
