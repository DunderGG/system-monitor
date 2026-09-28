#pragma once

#include <memory>
#include <sstream>
#include <string>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

namespace sysmon::tests
{

/**
 * Routes the default spdlog logger to an in-memory stream for its lifetime and
 * restores the previous logger on destruction. Each record is one
 * "level message" line, e.g. "warning Query failed", at every level including
 * trace. Not thread-safe; use only while no other thread is logging.
 */
class ScopedLogCapture
{
    static constexpr const char* kLoggerName = "test_log_capture";

public:
    ScopedLogCapture() : m_previousLogger(spdlog::default_logger())
    {
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_st>(m_output);
        auto logger = std::make_shared<spdlog::logger>(kLoggerName, sink);
        // Explicit "\n": spdlog's default line ending on Windows is "\r\n".
        logger->set_formatter(
            std::make_unique<spdlog::pattern_formatter>("%l %v", spdlog::pattern_time_type::local, "\n"));
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(std::move(logger));
    }

    ~ScopedLogCapture()
    {
        spdlog::set_default_logger(m_previousLogger);
        // set_default_logger() keeps the replaced logger registered. Drop it, or
        // the registry would hold a logger that writes to the destroyed m_output.
        spdlog::drop(kLoggerName);
    }

    ScopedLogCapture(const ScopedLogCapture&) = delete;
    ScopedLogCapture& operator=(const ScopedLogCapture&) = delete;
    ScopedLogCapture(ScopedLogCapture&&) = delete;
    ScopedLogCapture& operator=(ScopedLogCapture&&) = delete;

    /** Returns everything logged so far. */
    [[nodiscard]] std::string output() const
    {
        return m_output.str();
    }

private:
    std::ostringstream m_output;
    std::shared_ptr<spdlog::logger> m_previousLogger;
};

} // namespace sysmon::tests
