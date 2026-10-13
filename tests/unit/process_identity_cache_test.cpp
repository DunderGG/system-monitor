#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_identity_cache.h"

using namespace sysmon::domain;
using namespace sysmon::platform;

namespace
{

std::chrono::system_clock::time_point createdAt(int seconds)
{
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}

ProcessInfo process(uint32_t pid, int createdSeconds, std::string imageName = "app.exe")
{
    return ProcessInfo{.pid = pid, .creationTime = createdAt(createdSeconds), .imageName = std::move(imageName)};
}

// Returns details derived from the process, and records every call.
class RecordingDetailsReader
{
public:
    ProcessDetails operator()(const ProcessInfo& info)
    {
        reads.push_back(info.identity());
        return ProcessDetails{
            .imagePath = "C:\\Apps\\" + info.imageName,
            .commandLine =
                info.imageName + " --created " + std::to_string(info.creationTime.time_since_epoch().count()),
            .status = ProcessDetailsStatus::Available,
        };
    }

    std::vector<ProcessIdentity> reads;
};

} // namespace

TEST(ProcessIdentityCache, Apply_NewProcess_ReadsAndFillsDetails)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> processes{process(100, 10)};

    cache.apply(processes);

    ASSERT_EQ(reader.reads.size(), 1u);
    EXPECT_EQ(processes[0].imagePath, "C:\\Apps\\app.exe");
    EXPECT_TRUE(processes[0].commandLine.has_value());
    EXPECT_EQ(processes[0].detailsStatus, ProcessDetailsStatus::Available);
    EXPECT_EQ(cache.size(), 1u);
}

TEST(ProcessIdentityCache, Apply_SameProcessInLaterList_UsesCacheWithoutReading)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> first{process(100, 10), process(200, 20)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, 10), process(200, 20)};
    cache.apply(second);

    EXPECT_EQ(reader.reads.size(), 2u);
    EXPECT_EQ(second[0].imagePath, first[0].imagePath);
    EXPECT_EQ(second[1].commandLine, first[1].commandLine);
    EXPECT_EQ(second[1].detailsStatus, ProcessDetailsStatus::Available);
}

TEST(ProcessIdentityCache, Apply_PidReusedWithNewCreationTime_ReadsNewProcess)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> before{process(100, 10, "old.exe")};
    cache.apply(before);

    std::vector<ProcessInfo> after{process(100, 99, "new.exe")};
    cache.apply(after);

    ASSERT_EQ(reader.reads.size(), 2u);
    EXPECT_EQ(reader.reads[1], (ProcessIdentity{.pid = 100, .creationTime = createdAt(99)}));
    EXPECT_EQ(after[0].imagePath, "C:\\Apps\\new.exe");
    EXPECT_EQ(cache.size(), 1u);
}

TEST(ProcessIdentityCache, Apply_ProcessExited_EvictsItsEntry)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> first{process(100, 10), process(200, 20), process(300, 30)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, 10), process(300, 30)};
    cache.apply(second);

    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(reader.reads.size(), 3u);
}

TEST(ProcessIdentityCache, Apply_FailedRead_CachedAndNotRetried)
{
    int reads = 0;
    ProcessIdentityCache cache([&reads](const ProcessInfo&) {
        ++reads;
        return ProcessDetails{.imagePath = "C:\\Windows\\System32\\svchost.exe",
                              .status = ProcessDetailsStatus::AccessDenied};
    });
    std::vector<ProcessInfo> first{process(100, 10)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, 10)};
    cache.apply(second);

    EXPECT_EQ(reads, 1);
    EXPECT_EQ(second[0].detailsStatus, ProcessDetailsStatus::AccessDenied);
    EXPECT_EQ(second[0].imagePath, "C:\\Windows\\System32\\svchost.exe");
    EXPECT_FALSE(second[0].commandLine.has_value());
}

TEST(ProcessIdentityCache, Apply_NoReader_LeavesDetailsNotQueried)
{
    ProcessIdentityCache cache(nullptr);
    std::vector<ProcessInfo> processes{process(100, 10)};

    cache.apply(processes);

    EXPECT_FALSE(processes[0].imagePath.has_value());
    EXPECT_FALSE(processes[0].commandLine.has_value());
    EXPECT_EQ(processes[0].detailsStatus, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessIdentityCache, Apply_EmptyList_EvictsEverything)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> first{process(100, 10), process(200, 20)};
    cache.apply(first);

    std::vector<ProcessInfo> empty;
    cache.apply(empty);

    EXPECT_EQ(cache.size(), 0u);
}
