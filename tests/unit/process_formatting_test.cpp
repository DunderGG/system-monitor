#include <cstddef>
#include <optional>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "ui/process_formatting.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

TEST(ProcessFormatting, ProcessDisplayName_NamedProcess_IsImageName)
{
    EXPECT_EQ(processDisplayName(ProcessInfo{.pid = 1234, .imageName = "explorer.exe"}), "explorer.exe");
}

TEST(ProcessFormatting, ProcessDisplayName_PidZeroWithoutName_IsSystemIdleProcess)
{
    EXPECT_EQ(processDisplayName(ProcessInfo{.pid = 0}), "System Idle Process");
}

TEST(ProcessFormatting, ProcessDisplayName_OtherProcessWithoutName_IsNotAvailable)
{
    EXPECT_EQ(processDisplayName(ProcessInfo{.pid = 1234}), "N/A");
}

TEST(ProcessFormatting, ProcessListSummary_WithList_CountsProcesses)
{
    EXPECT_EQ(processListSummary(std::size_t{1234}, false), "Processes: 1,234");
    EXPECT_EQ(processListSummary(std::size_t{0}, true), "Processes: 0");
}

TEST(ProcessFormatting, ProcessListSummary_BeforeFirstList_IsPlaceholder)
{
    EXPECT_EQ(processListSummary(std::nullopt, false), "Processes: --");
}

TEST(ProcessFormatting, ProcessListSummary_ListMissingAfterEarlierOne_SaysNotAvailable)
{
    EXPECT_EQ(processListSummary(std::nullopt, true), "Processes: N/A (showing the last list read)");
}
