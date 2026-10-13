#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_collector.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

TEST(ProcessCollector, Collect_ListRead_ReturnsProcesses)
{
    ProcessCollector collector([] {
        return std::vector<ProcessInfo>{
            ProcessInfo{.pid = 4, .imageName = "System"},
            ProcessInfo{.pid = 1234, .parentPid = 4, .imageName = "app.exe"},
        };
    });

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 2u);
    EXPECT_EQ((*processes)[1].pid, 1234u);
    EXPECT_EQ((*processes)[1].imageName, "app.exe");
}

TEST(ProcessCollector, Collect_WithDetailsReader_ReadsEachProcessOnce)
{
    int detailReads = 0;
    ProcessCollector collector(
        [] {
            return std::vector<ProcessInfo>{
                ProcessInfo{.pid = 1234, .imageName = "app.exe"},
                ProcessInfo{.pid = 5678, .imageName = "tool.exe"},
            };
        },
        [&detailReads](const ProcessInfo& process) {
            ++detailReads;
            return ProcessDetails{
                .imagePath = "C:\\Apps\\" + process.imageName,
                .commandLine = process.imageName,
                .status = ProcessDetailsStatus::Available,
            };
        });

    (void)collector.collect();
    const auto processes = collector.collect();

    EXPECT_EQ(detailReads, 2);
    ASSERT_TRUE(processes.has_value());
    EXPECT_EQ((*processes)[1].imagePath, "C:\\Apps\\tool.exe");
    EXPECT_EQ((*processes)[1].detailsStatus, ProcessDetailsStatus::Available);
}

TEST(ProcessCollector, Collect_LsassAndCsrss_NeverPassedToDetailsReader)
{
    std::vector<std::string> readNames;
    ProcessCollector collector(
        [] {
            return std::vector<ProcessInfo>{
                ProcessInfo{.pid = 700, .imageName = "csrss.exe"},
                ProcessInfo{.pid = 900, .imageName = "lsass.exe"},
                ProcessInfo{.pid = 1234, .imageName = "app.exe"},
            };
        },
        [&readNames](const ProcessInfo& process) {
            readNames.push_back(process.imageName);
            return ProcessDetails{.imagePath = "C:\\Apps\\" + process.imageName,
                                  .commandLine = process.imageName,
                                  .status = ProcessDetailsStatus::Available};
        });

    const auto processes = collector.collect();

    EXPECT_EQ(readNames, std::vector<std::string>{"app.exe"});
    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 3u);
    for (std::size_t i = 0; i < 2; ++i) {
        EXPECT_EQ((*processes)[i].detailsStatus, ProcessDetailsStatus::NotQueried);
        EXPECT_FALSE((*processes)[i].imagePath.has_value());
    }
    EXPECT_EQ((*processes)[2].detailsStatus, ProcessDetailsStatus::Available);
}

TEST(ProcessCollector, Collect_NoDetailsReader_DetailsNotQueried)
{
    ProcessCollector collector([] { return std::vector<ProcessInfo>{ProcessInfo{.pid = 1234}}; });

    const auto processes = collector.collect();

    ASSERT_TRUE(processes.has_value());
    EXPECT_FALSE((*processes)[0].imagePath.has_value());
    EXPECT_EQ((*processes)[0].detailsStatus, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessCollector, Collect_ListReadFails_ReturnsNullopt)
{
    ProcessCollector collector([]() -> std::optional<std::vector<ProcessInfo>> { return std::nullopt; });

    EXPECT_FALSE(collector.collect().has_value());
}

TEST(ProcessCollector, Collect_NoReader_ReturnsNullopt)
{
    ProcessCollector collector(nullptr);

    EXPECT_FALSE(collector.collect().has_value());
}
