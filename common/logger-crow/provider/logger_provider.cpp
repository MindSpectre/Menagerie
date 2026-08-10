#include "logger_provider.hpp"

#include <menagerie/spider>

#include <detailed_entry.hpp>

void menagerie::crow::ComponentLoggerManager::initialize() {
    try {
        if (const std::shared_ptr<Logger> spider_logger = spider::instance().get<Logger>()) {
            logger_ = spider_logger;
            return;
        }
    } catch (...) {
        // TODO: process error
        //  Spider not available or logger not registered
    }
    // Fallback: create our own logger
    logger_ = std::make_shared<Logger>();

    // Default configuration: Console sink with colored output
    logger_->add_sink(std::make_unique<ConsoleSink<DetailedEntry>>(ConsoleSinkConfig::Builder{}
                                                                       .threshold(LogLevel::Debug)
                                                                       .enable_colors(true)
                                                                       .flush_each_entry(false)
                                                                       .finalize()));
}
