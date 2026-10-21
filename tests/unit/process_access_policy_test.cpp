#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_access_policy.h"

using namespace verdandi::domain;
using namespace verdandi::platform;

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_LsassCsrssAndNamelessProcessesButIdle)
{
    struct Case
    {
        const char* imageName;
        uint32_t pid;
        bool expected;
    };
    const std::array cases{
        Case{"lsass.exe", 900, true},
        Case{"csrss.exe", 900, true},
        Case{"LSASS.EXE", 900, true},
        Case{"Csrss.Exe", 900, true},
        Case{"lsass", 900, false},
        Case{"lsass.exe.bak", 900, false},
        Case{"mylsass.exe", 900, false},
        Case{"lsaiso.exe", 900, false},
        Case{"svchost.exe", 900, false},
        // A process without a name could be lsass.exe or csrss.exe, whatever its PID.
        Case{"", 4, true},
        Case{"", 900, true},
        Case{"", 1234, true},
        // The Idle process has no name.
        Case{"", 0, false},
    };

    for (const auto& [imageName, pid, expected] : cases) {
        SCOPED_TRACE(testing::Message() << "\"" << imageName << "\" (PID " << pid << ")");
        EXPECT_EQ(isNeverOpenedProcess(ProcessInfo{.pid = pid, .imageName = imageName}), expected);
    }
}

TEST(ProcessAccessPolicy, SkippingNeverOpenedProcesses_Lsass_ReaderNotCalledAndNotQueried)
{
    int reads = 0;
    const auto reader = skippingNeverOpenedProcesses([&reads](const ProcessInfo&) {
        ++reads;
        return ProcessDetails{.imagePath = "C:\\Windows\\System32\\lsass.exe",
                              .status = ProcessDetailsStatus::Available};
    });

    const ProcessDetails details = reader(ProcessInfo{.pid = 900, .imageName = "lsass.exe"});

    EXPECT_EQ(reads, 0);
    EXPECT_FALSE(details.imagePath.has_value());
    EXPECT_FALSE(details.commandLine.has_value());
    EXPECT_EQ(details.status, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessAccessPolicy, SkippingNeverOpenedProcesses_EmptyName_ReaderNotCalledAndNotQueried)
{
    int reads = 0;
    const auto reader = skippingNeverOpenedProcesses([&reads](const ProcessInfo&) {
        ++reads;
        return ProcessDetails{.imagePath = "C:\\Windows\\System32\\lsass.exe",
                              .status = ProcessDetailsStatus::Available};
    });

    const ProcessDetails details = reader(ProcessInfo{.pid = 900});

    EXPECT_EQ(reads, 0);
    EXPECT_FALSE(details.imagePath.has_value());
    EXPECT_EQ(details.status, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessAccessPolicy, SkippingNeverOpenedProcesses_IdleProcess_PassedToReader)
{
    int reads = 0;
    const auto reader = skippingNeverOpenedProcesses([&reads](const ProcessInfo&) {
        ++reads;
        return ProcessDetails{.status = ProcessDetailsStatus::Unavailable};
    });

    const ProcessDetails details = reader(ProcessInfo{.pid = 0});

    EXPECT_EQ(reads, 1);
    EXPECT_EQ(details.status, ProcessDetailsStatus::Unavailable);
}

TEST(ProcessAccessPolicy, SkippingNeverOpenedProcesses_OtherProcess_PassedToReader)
{
    int reads = 0;
    const auto reader = skippingNeverOpenedProcesses([&reads](const ProcessInfo&) {
        ++reads;
        return ProcessDetails{.status = ProcessDetailsStatus::AccessDenied};
    });

    const ProcessDetails details = reader(ProcessInfo{.pid = 1200, .imageName = "svchost.exe"});

    EXPECT_EQ(reads, 1);
    EXPECT_EQ(details.status, ProcessDetailsStatus::AccessDenied);
}

TEST(ProcessAccessPolicy, SkippingNeverOpenedProcesses_NullReader_StaysNull)
{
    EXPECT_FALSE(skippingNeverOpenedProcesses(nullptr));
}
