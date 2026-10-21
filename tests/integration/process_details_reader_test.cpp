#include <chrono>
#include <cstdint>
#include <string>

#include <windows.h>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_details_reader.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/utf8_conversion.h"

using namespace verdandi::domain;
using namespace verdandi::platform;

namespace
{

std::wstring thisExecutablePath()
{
    std::wstring path(MAX_PATH, L'\0');
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    path.resize(length);
    return path;
}

ProcessInfo thisProcess()
{
    FILETIME creation{};
    FILETIME exit{};
    FILETIME kernel{};
    FILETIME user{};
    ::GetProcessTimes(::GetCurrentProcess(), &creation, &exit, &kernel, &user);
    const auto creationTicks =
        static_cast<int64_t>((static_cast<uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime);
    return ProcessInfo{
        .pid = ::GetCurrentProcessId(),
        .creationTime = fileTimeToSystemClock(creationTicks),
        .imageName = "system_monitor_tests.exe",
    };
}

} // namespace

TEST(ProcessDetailsReaderIntegration, RealHost_ThisProcess_ReadsPathAndCommandLine)
{
    const ProcessDetailsReader reader = makeWindowsProcessDetailsReader();

    const ProcessDetails details = reader(thisProcess());

    EXPECT_EQ(details.status, ProcessDetailsStatus::Available);
    EXPECT_EQ(details.imagePath, wideToUtf8(thisExecutablePath()));
    EXPECT_EQ(details.commandLine, wideToUtf8(::GetCommandLineW()));
}

TEST(ProcessDetailsReaderIntegration, RealHost_PidReusedByAnotherProcess_DetailsNotAttributed)
{
    const ProcessDetailsReader reader = makeWindowsProcessDetailsReader();
    ProcessInfo earlierProcess = thisProcess();
    // The same PID with an earlier creation time stands for a process that
    // exited before its details were read, its PID now held by this one.
    earlierProcess.creationTime -= std::chrono::hours{1};

    const ProcessDetails details = reader(earlierProcess);

    EXPECT_EQ(details.status, ProcessDetailsStatus::Unavailable);
    EXPECT_FALSE(details.imagePath.has_value());
    EXPECT_FALSE(details.commandLine.has_value());
}

TEST(ProcessDetailsReaderIntegration, RealHost_IdleProcess_Unavailable)
{
    const ProcessDetailsReader reader = makeWindowsProcessDetailsReader();

    const ProcessDetails details = reader(ProcessInfo{.pid = 0});

    EXPECT_EQ(details.status, ProcessDetailsStatus::Unavailable);
    EXPECT_FALSE(details.imagePath.has_value());
}

TEST(ProcessDetailsReaderIntegration, RealHost_LsassByName_NotQueried)
{
    const ProcessDetailsReader reader = makeWindowsProcessDetailsReader();

    // Refused by name before OpenProcess; this process's PID makes sure a
    // missing check would show up as details being read.
    const ProcessDetails details = reader(ProcessInfo{
        .pid = ::GetCurrentProcessId(), .creationTime = thisProcess().creationTime, .imageName = "lsass.exe"});

    EXPECT_EQ(details.status, ProcessDetailsStatus::NotQueried);
    EXPECT_FALSE(details.imagePath.has_value());
    EXPECT_FALSE(details.commandLine.has_value());
}

TEST(ProcessDetailsReaderIntegration, RealHost_ProcessWithoutName_NotQueried)
{
    const ProcessDetailsReader reader = makeWindowsProcessDetailsReader();
    // Without a name it could be lsass.exe or csrss.exe. This process's PID
    // makes sure a missing check would show up as details being read.
    ProcessInfo unnamed = thisProcess();
    unnamed.imageName.clear();

    const ProcessDetails details = reader(unnamed);

    EXPECT_EQ(details.status, ProcessDetailsStatus::NotQueried);
    EXPECT_FALSE(details.imagePath.has_value());
    EXPECT_FALSE(details.commandLine.has_value());
}
