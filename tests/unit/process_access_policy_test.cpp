#include <cstdint>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_access_policy.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

ProcessInfo named(std::string imageName, uint32_t pid = 900)
{
    return ProcessInfo{.pid = pid, .imageName = std::move(imageName)};
}

} // namespace

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_LsassAndCsrss_True)
{
    EXPECT_TRUE(isNeverOpenedProcess(named("lsass.exe")));
    EXPECT_TRUE(isNeverOpenedProcess(named("csrss.exe")));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_DifferentCase_True)
{
    EXPECT_TRUE(isNeverOpenedProcess(named("LSASS.EXE")));
    EXPECT_TRUE(isNeverOpenedProcess(named("Csrss.Exe")));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_OtherNames_False)
{
    EXPECT_FALSE(isNeverOpenedProcess(named("lsass")));
    EXPECT_FALSE(isNeverOpenedProcess(named("lsass.exe.bak")));
    EXPECT_FALSE(isNeverOpenedProcess(named("mylsass.exe")));
    EXPECT_FALSE(isNeverOpenedProcess(named("lsaiso.exe")));
    EXPECT_FALSE(isNeverOpenedProcess(named("svchost.exe")));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_EmptyName_TrueWhateverThePid)
{
    // The name could have been lsass.exe's or csrss.exe's.
    EXPECT_TRUE(isNeverOpenedProcess(named("", 4)));
    EXPECT_TRUE(isNeverOpenedProcess(named("", 900)));
    EXPECT_TRUE(isNeverOpenedProcess(ProcessInfo{.pid = 1234}));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_IdleProcessWithoutName_False)
{
    EXPECT_FALSE(isNeverOpenedProcess(named("", 0)));
    EXPECT_FALSE(isNeverOpenedProcess(ProcessInfo{.pid = 0}));
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
