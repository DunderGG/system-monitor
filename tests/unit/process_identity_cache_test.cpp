#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/process_identity_cache.h"
#include "support/process_samples.h"

using namespace verdandi::domain;
using namespace verdandi::platform;
using namespace std::chrono_literals;
using verdandi::tests::process;

namespace
{

std::chrono::system_clock::time_point createdAt(int seconds)
{
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
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
    std::vector<ProcessInfo> processes{process(100, "app.exe", 10s)};

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
    std::vector<ProcessInfo> first{process(100, "app.exe", 10s), process(200, "app.exe", 20s)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, "app.exe", 10s), process(200, "app.exe", 20s)};
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
    std::vector<ProcessInfo> before{process(100, "old.exe", 10s)};
    cache.apply(before);

    std::vector<ProcessInfo> after{process(100, "new.exe", 99s)};
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
    std::vector<ProcessInfo> first{process(100, "app.exe", 10s), process(200, "app.exe", 20s),
                                   process(300, "app.exe", 30s)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, "app.exe", 10s), process(300, "app.exe", 30s)};
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
    std::vector<ProcessInfo> first{process(100, "app.exe", 10s)};
    cache.apply(first);

    std::vector<ProcessInfo> second{process(100, "app.exe", 10s)};
    cache.apply(second);

    EXPECT_EQ(reads, 1);
    EXPECT_EQ(second[0].detailsStatus, ProcessDetailsStatus::AccessDenied);
    EXPECT_EQ(second[0].imagePath, "C:\\Windows\\System32\\svchost.exe");
    EXPECT_FALSE(second[0].commandLine.has_value());
}

TEST(ProcessIdentityCache, Apply_NoReader_LeavesDetailsNotQueried)
{
    ProcessIdentityCache cache(nullptr);
    std::vector<ProcessInfo> processes{process(100, "app.exe", 10s)};

    cache.apply(processes);

    EXPECT_FALSE(processes[0].imagePath.has_value());
    EXPECT_FALSE(processes[0].commandLine.has_value());
    EXPECT_EQ(processes[0].detailsStatus, ProcessDetailsStatus::NotQueried);
}

TEST(ProcessIdentityCache, Apply_EmptyList_EvictsEverything)
{
    RecordingDetailsReader reader;
    ProcessIdentityCache cache([&reader](const ProcessInfo& info) { return reader(info); });
    std::vector<ProcessInfo> first{process(100, "app.exe", 10s), process(200, "app.exe", 20s)};
    cache.apply(first);

    std::vector<ProcessInfo> empty;
    cache.apply(empty);

    EXPECT_EQ(cache.size(), 0u);
}
