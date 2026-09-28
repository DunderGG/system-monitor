#include <string>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "log_capture.h"
#include "platform/windows/repeated_failure_log.h"

using sysmon::platform::RepeatedFailureLog;

namespace
{

class RepeatedFailureLogTest : public ::testing::Test
{
protected:
    [[nodiscard]] std::string output() const
    {
        return m_logCapture.output();
    }

private:
    sysmon::tests::ScopedLogCapture m_logCapture;
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
