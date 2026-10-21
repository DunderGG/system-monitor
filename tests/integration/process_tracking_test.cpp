#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <windows.h>

#include <gtest/gtest.h>

#include "current_process_elevation.h"
#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "platform/windows/process_access_policy.h"
#include "platform/windows/process_collector.h"
#include "platform/windows/process_details_reader.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/system_process_information_reader.h"
#include "suspended_child_process.h"

using namespace verdandi::domain;
using namespace verdandi::platform;
using verdandi::tests::isCurrentProcessElevated;
using verdandi::tests::SuspendedChildProcess;

namespace
{

// A printable, ordered form of a process identity for set comparisons.
using IdentityKey = std::pair<uint32_t, int64_t>;

IdentityKey keyOf(const ProcessIdentity& identity)
{
    return {identity.pid, identity.creationTime.time_since_epoch().count()};
}

std::string lowercaseAscii(std::string text)
{
    std::ranges::transform(text, text.begin(), [](char character) {
        return (character >= 'A' && character <= 'Z') ? static_cast<char>(character - 'A' + 'a') : character;
    });
    return text;
}

const ProcessInfo* findIdentity(const std::vector<ProcessInfo>& processes, const ProcessIdentity& identity)
{
    const auto it = std::ranges::find_if(
        processes, [&identity](const ProcessInfo& process) { return process.identity() == identity; });
    return it != processes.end() ? &*it : nullptr;
}

std::optional<std::vector<ProcessInfo>> readProcessList(SystemProcessInformationReader& reader)
{
    const auto bytes = reader.read();
    if (!bytes) {
        return std::nullopt;
    }
    return parseSystemProcessInformation(*bytes);
}

} // namespace

TEST(ProcessTrackingIntegration, RealHost_NewChildProcess_ListedWithIdentityAndDetails)
{
    ProcessCollector collector;
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    const ProcessInfo* listed = findIdentity(*processes, child.identity());
    ASSERT_NE(listed, nullptr);
    EXPECT_EQ(listed->parentPid, ::GetCurrentProcessId());
    EXPECT_EQ(listed->imageName, "cmd.exe");
    EXPECT_EQ(listed->detailsStatus, ProcessDetailsStatus::Available);
    ASSERT_TRUE(listed->imagePath.has_value());
    // Windows paths ignore case, and GetSystemDirectoryW may return "C:\WINDOWS\system32".
    EXPECT_EQ(lowercaseAscii(*listed->imagePath), lowercaseAscii(child.imagePath()));
    EXPECT_EQ(listed->commandLine, child.commandLine());
    EXPECT_GE(listed->threadCount, 1u);
}

TEST(ProcessTrackingIntegration, RealHost_ChildProcessExits_DisappearsFromList)
{
    ProcessCollector collector;
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());
    const auto before = collector.collect();
    ASSERT_TRUE(before.has_value());
    ASSERT_NE(findIdentity(*before, child.identity()), nullptr);

    child.terminate();

    // The list can take a moment to drop a process that has exited. A handle
    // another process holds does not keep it listed, so this wait is short.
    bool isGone = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!isGone && std::chrono::steady_clock::now() < deadline) {
        const auto after = collector.collect();
        ASSERT_TRUE(after.has_value());
        isGone = findIdentity(*after, child.identity()) == nullptr;
        if (!isGone) {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
        }
    }
    EXPECT_TRUE(isGone);
}

TEST(ProcessTrackingIntegration, RealHost_SecondCollect_ReadsDetailsOnlyForNewProcesses)
{
    SystemProcessInformationReader listReader;
    const ProcessDetailsReader windowsReader = makeWindowsProcessDetailsReader();
    std::set<IdentityKey> reads;
    ProcessCollector collector([&listReader] { return readProcessList(listReader); },
                               [&reads, &windowsReader](const ProcessInfo& process) {
                                   reads.insert(keyOf(process.identity()));
                                   return windowsReader(process);
                               });

    const auto first = collector.collect();
    ASSERT_TRUE(first.has_value());
    std::set<IdentityKey> firstIdentities;
    std::set<IdentityKey> expectedFirstReads;
    for (const auto& process : *first) {
        firstIdentities.insert(keyOf(process.identity()));
        if (!isNeverOpenedProcess(process)) {
            expectedFirstReads.insert(keyOf(process.identity()));
        }
    }
    EXPECT_EQ(reads, expectedFirstReads);

    // Guarantees at least one process the first list did not have.
    SuspendedChildProcess child;
    ASSERT_TRUE(child.started());
    reads.clear();
    const auto second = collector.collect();

    ASSERT_TRUE(second.has_value());
    std::set<IdentityKey> expectedSecondReads;
    for (const auto& process : *second) {
        const IdentityKey key = keyOf(process.identity());
        if (!firstIdentities.contains(key) && !isNeverOpenedProcess(process)) {
            expectedSecondReads.insert(key);
        }
    }
    EXPECT_EQ(reads, expectedSecondReads);
    EXPECT_TRUE(reads.contains(keyOf(child.identity())));
}

TEST(ProcessTrackingIntegration, RealHost_DetailsStatus_MatchesDetailsRead)
{
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    for (const auto& process : *processes) {
        const bool hasBoth = process.imagePath.has_value() && process.commandLine.has_value();
        switch (process.detailsStatus) {
            case ProcessDetailsStatus::Available:
                EXPECT_TRUE(hasBoth) << process.imageName << " (PID " << process.pid << ")";
                break;
            case ProcessDetailsStatus::NotQueried:
                EXPECT_TRUE(isNeverOpenedProcess(process)) << process.imageName;
                EXPECT_FALSE(process.imagePath.has_value() || process.commandLine.has_value()) << process.imageName;
                break;
            case ProcessDetailsStatus::AccessDenied:
            case ProcessDetailsStatus::Unavailable:
                EXPECT_FALSE(hasBoth) << process.imageName << " (PID " << process.pid << ")";
                break;
        }
    }
}

TEST(ProcessTrackingIntegration, RealHost_Unelevated_DeniedProcessesStayListedWithTheirMetrics)
{
    if (isCurrentProcessElevated()) {
        GTEST_SKIP() << "Running elevated; processes may not deny access";
    }
    ProcessCollector collector;

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    int deniedCount = 0;
    for (const auto& process : *processes) {
        if (process.detailsStatus != ProcessDetailsStatus::AccessDenied) {
            continue;
        }
        ++deniedCount;
        // Everything from the process list is still there.
        EXPECT_FALSE(process.imageName.empty()) << "PID " << process.pid;
        EXPECT_GT(process.creationTime, fileTimeToSystemClock(0)) << process.imageName;
        EXPECT_GT(process.workingSetBytes + process.privateBytes, 0u) << process.imageName;
    }
    // Services running as SYSTEM refuse an unelevated caller.
    EXPECT_GE(deniedCount, 1);
}
