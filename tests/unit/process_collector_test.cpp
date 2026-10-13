#include <optional>
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
