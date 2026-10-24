#include <atomic>
#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include <windows.h>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QEventLoop>
#include <QLocalSocket>
#include <QString>

#include "app/instance_server.h"
#include "app/single_instance.h"
#include "platform/windows/instance_mutex.h"

// The instance mutex, the instance server and claimSingleInstance(), with
// real mutexes and named pipes. Each test names its own, so that neither the
// test processes running at once nor a monitor that is running meet them.

using verdandi::app::claimSingleInstance;
using verdandi::app::InstanceClaim;
using verdandi::app::InstanceClaimResult;
using verdandi::app::InstanceServer;
using verdandi::platform::InstanceMutex;

namespace
{

// Only reached when a test fails, so it allows for a busy machine.
constexpr std::chrono::seconds kTimeout{5};

// A name that no other test, test process or monitor uses.
std::string uniqueApplicationName()
{
    static std::atomic<int> count{0};
    return std::format("VerdandiTest-{}-{}", ::GetCurrentProcessId(), ++count);
}

// Handles this thread's events until isDone() is true, for at most kTimeout.
// Returns whether isDone() became true.
template <typename Predicate> bool handleEventsUntil(Predicate isDone)
{
    const QDeadlineTimer deadline(kTimeout);
    while (!isDone()) {
        if (deadline.hasExpired()) {
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return true;
}

// Runs task on a thread of its own, as a later start runs, while this thread
// handles its events, as the running monitor's UI thread does, and returns
// task's result. task waits for a server on this thread to answer.
template <typename Task> std::invoke_result_t<Task> runWhileHandlingEvents(Task task)
{
    std::optional<std::invoke_result_t<Task>> result;
    std::atomic<bool> isDone{false};
    {
        std::jthread thread([&result, &isDone, &task] {
            result = task();
            isDone = true;
        });
        handleEventsUntil([&isDone] { return isDone.load(); });
    }
    return std::move(*result);
}

} // namespace

TEST(InstanceMutexIntegration, Name_IsTheApplicationTheSessionAndTheUsersSid)
{
    DWORD sessionId = 0;
    ASSERT_TRUE(::ProcessIdToSessionId(::GetCurrentProcessId(), &sessionId));
    const std::string application = uniqueApplicationName();

    const InstanceMutex mutex(application);

    EXPECT_TRUE(mutex.name().starts_with(std::format("{}-{}-S-1-", application, sessionId))) << mutex.name();
}

TEST(InstanceMutexIntegration, SecondMutexOfAName_IsHeldElsewhereUntilTheFirstIsDestroyed)
{
    const std::string application = uniqueApplicationName();

    auto first = std::make_optional<InstanceMutex>(application);
    const InstanceMutex::State firstState = first->state();
    const InstanceMutex second(application);
    first.reset();
    const InstanceMutex afterFirst(application);

    EXPECT_EQ(firstState, InstanceMutex::State::Created);
    EXPECT_EQ(second.state(), InstanceMutex::State::HeldElsewhere);
    EXPECT_TRUE(second.problem().empty()) << second.problem();
    EXPECT_EQ(afterFirst.state(), InstanceMutex::State::Created);
}

TEST(InstanceServerIntegration, AskToBringToFront_ListeningServer_CallsItsHandlerOnceAndAnswers)
{
    const QString name = QString::fromStdString(uniqueApplicationName());
    int bringToFrontCalls = 0;
    InstanceServer server([&bringToFrontCalls] { ++bringToFrontCalls; });
    ASSERT_EQ(server.listen(name), std::nullopt);

    const bool answered = runWhileHandlingEvents([&name] { return InstanceServer::askToBringToFront(name, kTimeout); });

    EXPECT_TRUE(answered);
    EXPECT_EQ(bringToFrontCalls, 1);
}

TEST(InstanceServerIntegration, UnknownRequest_DisconnectsWithoutAnswering)
{
    const QString name = QString::fromStdString(uniqueApplicationName());
    int bringToFrontCalls = 0;
    InstanceServer server([&bringToFrontCalls] { ++bringToFrontCalls; });
    ASSERT_EQ(server.listen(name), std::nullopt);
    QLocalSocket socket;
    socket.connectToServer(name);
    ASSERT_EQ(socket.state(), QLocalSocket::ConnectedState);

    socket.write("bring everything to front\n");
    const bool disconnected = handleEventsUntil([&socket] { return socket.state() == QLocalSocket::UnconnectedState; });

    EXPECT_TRUE(disconnected);
    EXPECT_EQ(bringToFrontCalls, 0);
    EXPECT_EQ(socket.bytesAvailable(), 0);
}

TEST(SingleInstanceIntegration, NoInstanceRunning_ClaimsTheMutex)
{
    const InstanceClaim claim = claimSingleInstance(uniqueApplicationName(), kTimeout);

    EXPECT_EQ(claim.result, InstanceClaimResult::Claimed);
    ASSERT_TRUE(claim.mutex.has_value());
    EXPECT_EQ(claim.mutex->state(), InstanceMutex::State::Created);
}

TEST(SingleInstanceIntegration, InstanceRunning_AsksItsServerAndClaimsNothing)
{
    const std::string application = uniqueApplicationName();
    const InstanceClaim running = claimSingleInstance(application, kTimeout);
    ASSERT_TRUE(running.mutex.has_value());
    InstanceServer server([] {});
    ASSERT_EQ(server.listen(QString::fromStdString(running.mutex->name())), std::nullopt);

    const InstanceClaim later =
        runWhileHandlingEvents([&application] { return claimSingleInstance(application, kTimeout); });

    EXPECT_EQ(later.result, InstanceClaimResult::BroughtToFront);
    EXPECT_FALSE(later.mutex.has_value());
}

TEST(SingleInstanceIntegration, InstanceRunningWithoutServer_NotAnsweredOnceTheTimeoutHasPassed)
{
    const std::string application = uniqueApplicationName();
    const InstanceMutex running(application);
    ASSERT_EQ(running.state(), InstanceMutex::State::Created);
    constexpr std::chrono::milliseconds kShortTimeout{300};
    const auto start = std::chrono::steady_clock::now();

    const InstanceClaim later = claimSingleInstance(application, kShortTimeout);

    EXPECT_EQ(later.result, InstanceClaimResult::NotAnswered);
    EXPECT_GE(std::chrono::steady_clock::now() - start, kShortTimeout);
    EXPECT_FALSE(later.mutex.has_value());
}

TEST(SingleInstanceIntegration, InstanceClosesWhileAsked_ClaimsTheMutexOnceItHasClosed)
{
    const std::string application = uniqueApplicationName();
    auto running = std::make_optional<InstanceMutex>(application);
    ASSERT_EQ(running->state(), InstanceMutex::State::Created);
    std::optional<InstanceClaim> later;

    {
        std::jthread laterStart([&later, &application] { later = claimSingleInstance(application, kTimeout); });
        // Long enough for the later start to have tried a few times.
        std::this_thread::sleep_for(std::chrono::milliseconds{300});
        running.reset();
    }

    ASSERT_TRUE(later.has_value());
    EXPECT_EQ(later->result, InstanceClaimResult::Claimed);
    EXPECT_TRUE(later->mutex.has_value());
}
