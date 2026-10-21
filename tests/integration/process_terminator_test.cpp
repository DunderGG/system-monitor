#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "current_process_elevation.h"
#include "domain/process_info.h"
#include "domain/process_termination.h"
#include "platform/windows/process_terminator.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/system_process_information_reader.h"
#include "suspended_child_process.h"

using namespace verdandi::domain;
using namespace verdandi::platform;
using verdandi::tests::isCurrentProcessElevated;
using verdandi::tests::SuspendedChildProcess;
using verdandi::tests::UnelevatedToken;

namespace
{

// Ample time for Windows to finish ending a suspended child.
constexpr std::chrono::milliseconds kExitTimeout{5000};
constexpr std::chrono::milliseconds kNoWait{0};

// The System process, which belongs to SYSTEM and always has PID 4.
constexpr uint32_t kSystemProcessId = 4;

std::optional<ProcessInfo> findSystemProcess()
{
    SystemProcessInformationReader reader;
    const auto bytes = reader.read();
    if (!bytes) {
        return std::nullopt;
    }
    const auto processes = parseSystemProcessInformation(*bytes);
    if (!processes) {
        return std::nullopt;
    }
    const auto it = std::ranges::find(*processes, kSystemProcessId, &ProcessInfo::pid);
    return it != processes->end() ? std::optional{*it} : std::nullopt;
}

} // namespace

// Children that the terminator should be able to end are created unelevated,
// so these tests also hold when they run elevated, as on CI runners.

TEST(ProcessTerminatorIntegration, RealHost_OwnUnelevatedChild_TerminatedWithExitCodeOne)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());

    const ProcessTerminationResult result = terminateOwnedProcess(child.processInfo());

    EXPECT_EQ(result, ProcessTerminationResult::Terminated);
    EXPECT_EQ(child.waitForExitCode(kExitTimeout), 1u);
}

TEST(ProcessTerminatorIntegration, RealHost_PidReusedByAnotherProcess_LeavesItRunning)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());
    ProcessInfo earlierProcess = child.processInfo();
    // The same PID with an earlier creation time stands for a process that
    // exited before the request, its PID now held by the child.
    earlierProcess.creationTime -= std::chrono::hours{1};

    const ProcessTerminationResult result = terminateOwnedProcess(earlierProcess);

    EXPECT_EQ(result, ProcessTerminationResult::AlreadyExited);
    EXPECT_FALSE(child.waitForExitCode(kNoWait).has_value());
}

TEST(ProcessTerminatorIntegration, RealHost_ChildAlreadyTerminated_AlreadyExited)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());
    ASSERT_EQ(terminateOwnedProcess(child.processInfo()), ProcessTerminationResult::Terminated);
    ASSERT_TRUE(child.waitForExitCode(kExitTimeout).has_value());

    // The test's handle keeps the exited child's process object, and its PID.
    const ProcessTerminationResult result = terminateOwnedProcess(child.processInfo());

    EXPECT_EQ(result, ProcessTerminationResult::AlreadyExited);
    EXPECT_EQ(child.waitForExitCode(kNoWait), 1u);
}

TEST(ProcessTerminatorIntegration, RealHost_ChildExitedAndHandlesClosed_AlreadyExited)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());
    const ProcessInfo exitedChild = child.processInfo();
    child.terminate();

    // The PID is now unused, held by an exited child that another process
    // (an antivirus scanner, say) still has open, or reused by a new process.
    const ProcessTerminationResult result = terminateOwnedProcess(exitedChild);

    EXPECT_EQ(result, ProcessTerminationResult::AlreadyExited);
}

TEST(ProcessTerminatorIntegration, RealHost_NeverOpenedImageName_RefusedAndLeftRunning)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());
    ProcessInfo namedLikeCsrss = child.processInfo();
    namedLikeCsrss.imageName = "csrss.exe";

    const ProcessTerminationResult result = terminateOwnedProcess(namedLikeCsrss);

    EXPECT_EQ(result, ProcessTerminationResult::NeverOpened);
    EXPECT_FALSE(child.waitForExitCode(kNoWait).has_value());
}

TEST(ProcessTerminatorIntegration, RealHost_ProcessWithoutName_RefusedAndLeftRunning)
{
    SuspendedChildProcess child(UnelevatedToken{});
    ASSERT_TRUE(child.started());
    // Only the identity, as a caller that does not pass the listed name would.
    const ProcessInfo identityOnly{.pid = child.processInfo().pid, .creationTime = child.processInfo().creationTime};

    const ProcessTerminationResult result = terminateOwnedProcess(identityOnly);

    EXPECT_EQ(result, ProcessTerminationResult::NeverOpened);
    EXPECT_FALSE(child.waitForExitCode(kNoWait).has_value());
}

TEST(ProcessTerminatorIntegration, RealHost_ElevatedOwnChild_RefusedAndLeftRunning)
{
    if (!isCurrentProcessElevated()) {
        GTEST_SKIP() << "Running unelevated; cannot start an elevated child";
    }
    // Created with this process's elevated token.
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());

    const ProcessTerminationResult result = terminateOwnedProcess(child.processInfo());

    EXPECT_EQ(result, ProcessTerminationResult::Elevated);
    EXPECT_FALSE(child.waitForExitCode(kNoWait).has_value());
}

TEST(ProcessTerminatorIntegration, RealHost_UnelevatedChildAtHighIntegrity_RefusedAndLeftRunning)
{
    if (!isCurrentProcessElevated()) {
        GTEST_SKIP() << "Running unelevated; cannot start a child at high integrity";
    }
    // Not elevated, but at high integrity, as a UIAccess program can be.
    SuspendedChildProcess child(UnelevatedToken{.integrityLevel = WinHighLabelSid});
    ASSERT_TRUE(child.started());

    const ProcessTerminationResult result = terminateOwnedProcess(child.processInfo());

    EXPECT_EQ(result, ProcessTerminationResult::Elevated);
    EXPECT_FALSE(child.waitForExitCode(kNoWait).has_value());
}

TEST(ProcessTerminatorIntegration, RealHost_IdleProcess_NotOwnedByCurrentUser)
{
    const ProcessTerminationResult result = terminateOwnedProcess(ProcessInfo{.pid = 0});

    EXPECT_EQ(result, ProcessTerminationResult::NotOwnedByCurrentUser);
}

TEST(ProcessTerminatorIntegration, RealHost_SystemProcess_NotTerminated)
{
    const std::optional<ProcessInfo> system = findSystemProcess();
    ASSERT_TRUE(system.has_value());

    const ProcessTerminationResult result = terminateOwnedProcess(*system);

    // Unelevated, Windows refuses to open it; elevated, its token names SYSTEM.
    EXPECT_TRUE(result == ProcessTerminationResult::AccessDenied ||
                result == ProcessTerminationResult::NotOwnedByCurrentUser)
        << "result " << static_cast<int>(result);
}
