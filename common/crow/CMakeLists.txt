set(CROW ${COMMON}.Crow)

##############################################################################
# Crow Entry
##############################################################################
add_library(${CROW}.Entry STATIC
        entry/interface/entry_interface.hpp
        entry/detail/log_level.hpp
        entry/factory/entry_factory.hpp
        entry/detailed_entry.hpp
        entry/light_entry.hpp
        entry/detailed_entry.cpp
)
target_include_directories(${CROW}.Entry PUBLIC
        entry/factory
        entry/interface
        entry/
)
target_link_libraries(${CROW}.Entry PUBLIC
        JsonCpp::JsonCpp
        Threads::Threads
        Menagerie::Common::Spider
        Menagerie::Common::Beavers
        Menagerie::Common::Chrono
)
##############################################################################

##############################################################################
# Sinks
##############################################################################
add_library(${CROW}.Sink INTERFACE
        sink/file/file_sink.hpp
        sink/file/config/file_sink_config.hpp
        sink/file/detail/file_naming.hpp
        sink/detail/prefix_filter.hpp
        sink/detail/log_event.hpp
        sink/detail/sink_status.hpp
        sink/console/console_sink.hpp
        sink/console/config/console_sink_config.hpp
)

target_include_directories(${CROW}.Sink INTERFACE
        sink/interface
        sink/console
        sink/file
        sink/
)

target_link_libraries(${CROW}.Sink INTERFACE
        ${CROW}.Entry
        Menagerie::Common::Chameleon
        Menagerie::Common::Serialization
)
##############################################################################

##############################################################################
# Crow logger
##############################################################################
add_library(${CROW}.Logger STATIC
        logger/logger.hpp
        logger/logger.cpp
        logger/config/logger_config.hpp
)
target_include_directories(${CROW}.Logger PUBLIC
        logger
)

target_link_libraries(${CROW}.Logger
        PUBLIC
        ${CROW}.Sink
        Boost::asio
        Boost::system
)
##############################################################################

##############################################################################
# Crow logger provider
##############################################################################
add_library(${CROW}.Logger.Provider STATIC
        provider/logger_provider.cpp
        provider/logger_provider.hpp
        provider/macros/log_macros_adds.hpp
        provider/macros/log_macros.hpp
)
target_include_directories(${CROW}.Logger.Provider PUBLIC
        provider/
)

target_link_libraries(${CROW}.Logger.Provider
        PUBLIC
        ${CROW}.Logger
)
##############################################################################


##############################################################################
# Crow exported lib
##############################################################################
add_combined_library(${CROW}
        DIRECTORIES
        export
        SOURCES
        export/menagerie/crow
        LIBRARIES
        ${CROW}.Logger.Provider
)

add_library(Menagerie::Common::Crow ALIAS ${CROW})
##############################################################################