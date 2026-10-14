#include <cstddef>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <QChar>
#include <QString>

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

TEST(ProcessFormatting, ProcessIoBytesPerSec_BothRates_Sum)
{
    EXPECT_EQ(processIoBytesPerSec(ProcessInfo{.ioReadBytesPerSec = 3000, .ioWriteBytesPerSec = 500}), 3500u);
}

TEST(ProcessFormatting, ProcessIoBytesPerSec_EitherRateMissing_Nullopt)
{
    EXPECT_FALSE(processIoBytesPerSec(ProcessInfo{.ioReadBytesPerSec = 3000}).has_value());
    EXPECT_FALSE(processIoBytesPerSec(ProcessInfo{.ioWriteBytesPerSec = 500}).has_value());
}

TEST(ProcessFormatting, ProcessIoToolTip_ShowsEachRate)
{
    EXPECT_EQ(processIoToolTip(ProcessInfo{.ioReadBytesPerSec = 2048, .ioWriteBytesPerSec = 0}),
              "Read: 2.0 KiB/s\nWrite: 0 B/s");
    EXPECT_EQ(processIoToolTip(ProcessInfo{}), "Read: N/A\nWrite: N/A");
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

TEST(ProcessFormatting, ProcessTerminationQuestion_NamedProcess_NamesItAndItsPid)
{
    EXPECT_EQ(processTerminationQuestion(ProcessInfo{.pid = 1234, .imageName = "explorer.exe"}),
              "Do you want to end explorer.exe (PID 1234)?");
}

TEST(ProcessFormatting, ProcessTerminationQuestion_NameWithPlaceholder_KeptLiterally)
{
    EXPECT_EQ(processTerminationQuestion(ProcessInfo{.pid = 7, .imageName = "a%2b%1.exe"}),
              "Do you want to end a%2b%1.exe (PID 7)?");
}

TEST(ProcessFormatting, ProcessDetailsToolTip_Available_ShowsPathAndCommandLine)
{
    const ProcessInfo process{
        .imageName = "app.exe",
        .imagePath = "C:\\Apps\\app.exe",
        .commandLine = "app.exe --mode \"fast\" <input>",
        .detailsStatus = ProcessDetailsStatus::Available,
    };

    EXPECT_EQ(processDetailsToolTip(process),
              "<p><b>Path:</b> C:\\Apps\\app.exe</p>"
              "<p><b>Command line:</b> app.exe --mode &quot;fast&quot; &lt;input&gt;</p>");
}

TEST(ProcessFormatting, ProcessDetailsToolTip_EachMissingStatus_SaysWhy)
{
    const auto toolTip = [](ProcessDetailsStatus status) {
        return processDetailsToolTip(ProcessInfo{.detailsStatus = status});
    };

    EXPECT_TRUE(toolTip(ProcessDetailsStatus::AccessDenied).startsWith("<p>Access denied:"));
    EXPECT_TRUE(toolTip(ProcessDetailsStatus::NotQueried).startsWith("<p>Not read:"));
    EXPECT_TRUE(toolTip(ProcessDetailsStatus::Unavailable).startsWith("<p>Unavailable:"));
    EXPECT_FALSE(toolTip(ProcessDetailsStatus::AccessDenied).contains("Path:"));
}

TEST(ProcessFormatting, ProcessDetailsToolTip_PartlyRead_ShowsWhatWasRead)
{
    const ProcessInfo process{
        .imagePath = "C:\\Apps\\app.exe",
        .detailsStatus = ProcessDetailsStatus::AccessDenied,
    };

    const QString toolTip = processDetailsToolTip(process);

    EXPECT_TRUE(toolTip.startsWith("<p>Access denied:"));
    EXPECT_TRUE(toolTip.endsWith("<p><b>Path:</b> C:\\Apps\\app.exe</p>"));
    EXPECT_FALSE(toolTip.contains("Command line"));
}

TEST(ProcessFormatting, ProcessDetailsToolTip_LongCommandLine_CutOffWithEllipsis)
{
    const ProcessInfo process{
        .commandLine = std::string(static_cast<std::size_t>(kToolTipDetailLength) + 500, 'x'),
        .detailsStatus = ProcessDetailsStatus::Available,
    };

    const QString toolTip = processDetailsToolTip(process);

    EXPECT_TRUE(toolTip.contains(QString(kToolTipDetailLength, 'x') + QChar(0x2026)));
    EXPECT_FALSE(toolTip.contains(QString(kToolTipDetailLength + 1, 'x')));
}
