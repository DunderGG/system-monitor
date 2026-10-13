#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_access_policy.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_LsassAndCsrss_True)
{
    EXPECT_TRUE(isNeverOpenedProcess("lsass.exe"));
    EXPECT_TRUE(isNeverOpenedProcess("csrss.exe"));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_DifferentCase_True)
{
    EXPECT_TRUE(isNeverOpenedProcess("LSASS.EXE"));
    EXPECT_TRUE(isNeverOpenedProcess("Csrss.Exe"));
}

TEST(ProcessAccessPolicy, IsNeverOpenedProcess_OtherNames_False)
{
    EXPECT_FALSE(isNeverOpenedProcess(""));
    EXPECT_FALSE(isNeverOpenedProcess("lsass"));
    EXPECT_FALSE(isNeverOpenedProcess("lsass.exe.bak"));
    EXPECT_FALSE(isNeverOpenedProcess("mylsass.exe"));
    EXPECT_FALSE(isNeverOpenedProcess("lsaiso.exe"));
    EXPECT_FALSE(isNeverOpenedProcess("svchost.exe"));
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
