#include <array>
#include <cstddef>
#include <optional>
#include <string>

#include <gtest/gtest.h>
#include <QChar>
#include <QString>

#include "domain/process_info.h"
#include "domain/process_termination.h"
#include "domain/settings.h"
#include "ui/dashboard_formatting.h"
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
    const ProcessInfo process{.ioReadBytesPerSec = 2048, .ioWriteBytesPerSec = 0};

    EXPECT_EQ(processIoToolTip(process, ByteUnits::Binary), "Read: 2.0 KiB/s\nWrite: 0 B/s");
    EXPECT_EQ(processIoToolTip(process, ByteUnits::Decimal),
              "Read: " + formatByteRate(2048, ByteUnits::Decimal) + "\nWrite: 0 B/s");
    EXPECT_EQ(processIoToolTip(ProcessInfo{}, ByteUnits::Binary), "Read: N/A\nWrite: N/A");
}

TEST(ProcessFormatting, ProcessMemoryToolTip_ShowsEachFigureWithWhatItCounts)
{
    const ProcessInfo browser{
        .workingSetBytes = 397'082'214,        // 378.7 MiB
        .privateWorkingSetBytes = 217'055'232, // 207.0 MiB
        .privateBytes = 262'144'000,           // 250.0 MiB
    };

    EXPECT_EQ(processMemoryToolTip(browser, ByteUnits::Binary),
              "Private working set: 207.0 MiB\n"
              "Working set: 378.7 MiB (includes memory shared with other processes)\n"
              "Commit size: 250.0 MiB (private memory committed, which counts toward the commit limit)");
    EXPECT_TRUE(processMemoryToolTip(browser, ByteUnits::Decimal)
                    .startsWith("Private working set: " + formatBytes(217'055'232, ByteUnits::Decimal) + "\n"));
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

TEST(ProcessFormatting, MonitorCloseQuestion_OwnRow_NamesItAndOffersToClose)
{
    EXPECT_EQ(monitorCloseQuestion(ProcessInfo{.pid = 1234, .imageName = "verdandi.exe"}),
              "verdandi.exe (PID 1234) is Verdandi itself. Do you want to close Verdandi?");
}

TEST(ProcessFormatting, ProcessTerminationMessage_Terminated_None)
{
    EXPECT_FALSE(processTerminationMessage(ProcessInfo{.pid = 1234, .imageName = "explorer.exe"},
                                           ProcessTerminationResult::Terminated)
                     .has_value());
}

TEST(ProcessFormatting, ProcessTerminationMessage_AlreadyExited_SaysSoAndThatAProcessWithItsPidWasLeftRunning)
{
    const auto message = processTerminationMessage(ProcessInfo{.pid = 1234, .imageName = "explorer.exe"},
                                                   ProcessTerminationResult::AlreadyExited);

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->text, "explorer.exe (PID 1234) has already exited.");
    EXPECT_TRUE(message->informativeText.contains("left running"));
}

TEST(ProcessFormatting, ProcessTerminationMessage_NotEnded_NamesTheProcessAndSaysWhy)
{
    struct Case
    {
        ProcessTerminationResult result;
        const char* reason;
    };
    const std::array cases{
        Case{ProcessTerminationResult::NotOwnedByCurrentUser, "runs as another user"},
        Case{ProcessTerminationResult::Elevated, "runs elevated"},
        Case{ProcessTerminationResult::NeverOpened, "critical Windows process"},
        Case{ProcessTerminationResult::AccessDenied, "denied access"},
        Case{ProcessTerminationResult::Failed, "log has the error code"},
    };

    for (const auto& [result, reason] : cases) {
        SCOPED_TRACE(reason);
        const auto message = processTerminationMessage(ProcessInfo{.pid = 1234, .imageName = "explorer.exe"}, result);

        ASSERT_TRUE(message.has_value());
        EXPECT_EQ(message->text, "Could not end explorer.exe (PID 1234).");
        EXPECT_TRUE(message->informativeText.contains(reason));
    }
}

TEST(ProcessFormatting, ProcessTerminationMessage_NeverOpenedWithoutName_SaysItCouldBeCritical)
{
    const auto message = processTerminationMessage(ProcessInfo{.pid = 1234}, ProcessTerminationResult::NeverOpened);

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->text, "Could not end N/A (PID 1234).");
    EXPECT_TRUE(message->informativeText.contains("without a name"));
    EXPECT_TRUE(message->informativeText.contains("could be a critical Windows process"));
}

TEST(ProcessFormatting, ProcessTerminationMessage_IdleProcess_NamesSystemIdleProcess)
{
    const auto message =
        processTerminationMessage(ProcessInfo{.pid = 0}, ProcessTerminationResult::NotOwnedByCurrentUser);

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->text, "Could not end System Idle Process (PID 0).");
}

TEST(ProcessFormatting, ProcessTerminationMessage_NameWithPlaceholder_KeptLiterally)
{
    const auto message =
        processTerminationMessage(ProcessInfo{.pid = 7, .imageName = "a%2b%1.exe"}, ProcessTerminationResult::Failed);

    ASSERT_TRUE(message.has_value());
    EXPECT_EQ(message->text, "Could not end a%2b%1.exe (PID 7).");
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

TEST(ProcessFormatting, ProcessDetailsToolTip_NotQueried_SaysWhyForANamedAndAnUnnamedProcess)
{
    const QString named = processDetailsToolTip(
        ProcessInfo{.pid = 900, .imageName = "lsass.exe", .detailsStatus = ProcessDetailsStatus::NotQueried});
    const QString unnamed =
        processDetailsToolTip(ProcessInfo{.pid = 900, .detailsStatus = ProcessDetailsStatus::NotQueried});

    EXPECT_TRUE(named.contains("security software"));
    EXPECT_FALSE(named.contains("without a name"));
    EXPECT_TRUE(unnamed.startsWith("<p>Not read:"));
    EXPECT_TRUE(unnamed.contains("without a name"));
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
