#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "platform/windows/repeated_failure_log.h"

using sysmon::platform::RepeatedFailureLog;

namespace
{

// Routes the default logger to a string stream for the duration of a test,
// one "level message" line per record.
class RepeatedFailureLogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_previousLogger = spdlog::default_logger();
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_st>(m_output);
        auto logger = std::make_shared<spdlog::logger>("repeated_failure_log_test", sink);
        // Explicit "\n": spdlog's default line ending on Windows is "\r\n".
        logger->set_formatter(
            std::make_unique<spdlog::pattern_formatter>("%l %v", spdlog::pattern_time_type::local, "\n"));
        logger->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger);
    }

    void TearDown() override
    {
        spdlog::set_default_logger(m_previousLogger);
    }

    [[nodiscard]] std::string output() const
    {
        return m_output.str();
    }

private:
    std::ostringstream m_output;
    std::shared_ptr<spdlog::logger> m_previousLogger;
};

} // namespace

TEST_F(RepeatedFailureLogTest, FirstFailure_LoggedAtRequestedLevel)
{
    RepeatedFailureLog failureLog("Query");

    failureLog.failure(spdlog::level::warn, "Query failed with error {}", 5);

    EXPECT_EQ(output(), "warning Query failed with error 5\n");
    EXPECT_EQ(failureLog.consecutiveFailures(), 1u);
}

TEST_F(RepeatedFailureLogTest, RepeatedFailures_LoggedAtDebug)
{
    RepeatedFailureLog failureLog("Query");

    failureLog.failure(spdlog::level::err, "Query failed with error {}", 5);
    failureLog.failure(spdlog::level::err, "Query failed with error {}", 5);
    failureLog.failure(spdlog::level::err, "Query failed with error {}", 5);

    EXPECT_EQ(output(), "error Query failed with error 5\n"
                        "debug Query failed with error 5\n"
                        "debug Query failed with error 5\n");
}

TEST_F(RepeatedFailureLogTest, SuccessAfterFailures_LogsRecoveryOnce)
{
    RepeatedFailureLog failureLog("Query");
    failureLog.failure(spdlog::level::warn, "Query failed");
    failureLog.failure(spdlog::level::warn, "Query failed");

    failureLog.success();
    failureLog.success();

    EXPECT_EQ(output(), "warning Query failed\n"
                        "debug Query failed\n"
                        "info Query succeeded again after 2 consecutive failures\n");
    EXPECT_EQ(failureLog.consecutiveFailures(), 0u);
}

TEST_F(RepeatedFailureLogTest, SuccessWithoutFailure_LogsNothing)
{
    RepeatedFailureLog failureLog("Query");

    failureLog.success();

    EXPECT_TRUE(output().empty());
}

TEST_F(RepeatedFailureLogTest, FailureAfterRecovery_LoggedAtRequestedLevelAgain)
{
    RepeatedFailureLog failureLog("Query");
    failureLog.failure(spdlog::level::warn, "Query failed");
    failureLog.success();

    failureLog.failure(spdlog::level::warn, "Query failed");

    EXPECT_EQ(output(), "warning Query failed\n"
                        "info Query succeeded again after 1 consecutive failures\n"
                        "warning Query failed\n");
}
