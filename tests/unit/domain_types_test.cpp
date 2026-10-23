#include <chrono>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/memory_sample.h"
#include "domain/network_profile.h"
#include "domain/network_sample.h"
#include "domain/process_identity.h"
#include "domain/process_info.h"
#include "domain/settings.h"
#include "domain/system_snapshot.h"

using namespace verdandi::domain;

// Default values that encode decisions: missing data is std::nullopt or
// Unknown, never a zero that would read as a measurement (DC-12, DC-39).

TEST(MemorySample, DefaultConstruction_CacheAndPoolSizesUnknown)
{
    const MemorySample sample;

    EXPECT_FALSE(sample.systemCacheBytes.has_value());
    EXPECT_FALSE(sample.pagedPoolBytes.has_value());
    EXPECT_FALSE(sample.nonPagedPoolBytes.has_value());
}

TEST(NetworkSample, DefaultConstruction_UnknownStatusAndNoRates)
{
    const NetworkSample sample;

    EXPECT_EQ(sample.operationalStatus, OperationalStatus::Unknown);
    EXPECT_FALSE(sample.inBytesPerSec.has_value());
    EXPECT_FALSE(sample.outBytesPerSec.has_value());
}

TEST(ConnectivityStatus, DefaultConstruction_UnknownWithUnknownCost)
{
    const ConnectivityStatus status;

    EXPECT_EQ(status.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(status.isMetered.has_value());
    EXPECT_FALSE(status.isRoaming.has_value());
}

TEST(NetworkProfile, DefaultConstruction_NoCategory)
{
    const NetworkProfile profile;

    EXPECT_FALSE(profile.category.has_value());
}

TEST(NetworkProfile, Equality_ComparesEveryField)
{
    const NetworkProfile home{.name = "Home", .category = NetworkCategory::Private, .interfaceIds = {1}};

    EXPECT_EQ(home, (NetworkProfile{.name = "Home", .category = NetworkCategory::Private, .interfaceIds = {1}}));
    EXPECT_NE(home, (NetworkProfile{.name = "Home", .category = NetworkCategory::Public, .interfaceIds = {1}}));
    EXPECT_NE(home, (NetworkProfile{.name = "Home", .category = NetworkCategory::Private, .interfaceIds = {2}}));
}

TEST(ProcessInfo, DefaultConstruction_DetailsNotQueriedAndRatesUnknown)
{
    const ProcessInfo process;

    EXPECT_EQ(process.detailsStatus, ProcessDetailsStatus::NotQueried);
    EXPECT_FALSE(process.imagePath.has_value());
    EXPECT_FALSE(process.commandLine.has_value());
    EXPECT_FALSE(process.cpuUsagePercent.has_value());
    EXPECT_FALSE(process.ioReadBytesPerSec.has_value());
    EXPECT_FALSE(process.ioWriteBytesPerSec.has_value());
}

// Two ProcessInfo values with the same PID but different creation times are
// distinct identities. The test encodes the design decision: callers must
// never use PID alone as a stable key.
TEST(ProcessInfo, SamePidDifferentCreationTime_DifferentIdentity)
{
    using std::chrono::seconds;
    using std::chrono::system_clock;
    const auto t1 = system_clock::time_point{seconds{1000}};
    const auto t2 = system_clock::time_point{seconds{2000}};

    ProcessInfo a{.pid = 42, .creationTime = t1, .imageName = "old.exe"};
    ProcessInfo b{.pid = 42, .creationTime = t2, .imageName = "new.exe"};

    // Same PID — but different identity
    EXPECT_EQ(a.pid, b.pid);
    EXPECT_NE(a.identity(), b.identity());
}

TEST(ProcessInfo, SamePidAndCreationTime_SameIdentityAndHash)
{
    const auto created = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};
    ProcessInfo before{.pid = 42, .creationTime = created, .threadCount = 3};
    ProcessInfo after{.pid = 42, .creationTime = created, .threadCount = 9};

    EXPECT_EQ(before.identity(), after.identity());
    EXPECT_EQ(ProcessIdentityHash{}(before.identity()), ProcessIdentityHash{}(after.identity()));
}

TEST(ProcessIdentity, DifferentPidSameCreationTime_NotEqual)
{
    const auto created = std::chrono::system_clock::time_point{std::chrono::seconds{1000}};

    EXPECT_NE((ProcessIdentity{.pid = 1, .creationTime = created}),
              (ProcessIdentity{.pid = 2, .creationTime = created}));
}

TEST(SystemSnapshot, DefaultConstruction_NoOptionalData)
{
    const SystemSnapshot snapshot;

    EXPECT_FALSE(snapshot.cpu.has_value());
    EXPECT_FALSE(snapshot.memory.has_value());
    EXPECT_FALSE(snapshot.disks.has_value());
    EXPECT_FALSE(snapshot.networks.has_value());
    EXPECT_EQ(snapshot.connectivity.level, ConnectivityLevel::Unknown);
    EXPECT_FALSE(snapshot.networkProfiles.has_value());
    EXPECT_FALSE(snapshot.uptime.has_value());
    EXPECT_EQ(snapshot.health, SystemHealth{});
    EXPECT_FALSE(snapshot.processes.has_value());
    EXPECT_FALSE(snapshot.activity.has_value());
}

TEST(Settings, NearestRefreshInterval_NearestHalfSecondWithinTheLimits)
{
    using std::chrono::milliseconds;
    struct Case
    {
        const char* name;
        milliseconds interval;
        milliseconds expected;
    };
    const std::vector<Case> cases{
        {"the shortest", milliseconds{500}, milliseconds{500}},
        {"a step", milliseconds{2500}, milliseconds{2500}},
        {"the longest", milliseconds{10'000}, milliseconds{10'000}},
        {"nearer the step below", milliseconds{1249}, milliseconds{1000}},
        {"halfway rounds up", milliseconds{1250}, milliseconds{1500}},
        {"nearer the step above", milliseconds{1300}, milliseconds{1500}},
        {"below the shortest", milliseconds{100}, milliseconds{500}},
        {"negative", milliseconds{-1000}, milliseconds{500}},
        {"above the longest", milliseconds{10'249}, milliseconds{10'000}},
        {"far above the longest", milliseconds::max(), milliseconds{10'000}},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        EXPECT_EQ(nearestRefreshInterval(testCase.interval), testCase.expected);
    }
}

TEST(Settings, LogLevelNames_EachLevelAndItsNameBothWays)
{
    struct Case
    {
        std::string_view name;
        LogLevel level;
    };
    const std::vector<Case> cases{
        {"trace", LogLevel::Trace}, {"debug", LogLevel::Debug}, {"info", LogLevel::Info},
        {"warn", LogLevel::Warn},   {"error", LogLevel::Error}, {"critical", LogLevel::Critical},
        {"off", LogLevel::Off},
    };
    for (const auto& testCase : cases) {
        SCOPED_TRACE(testCase.name);
        EXPECT_EQ(parseLogLevel(testCase.name), testCase.level);
        EXPECT_EQ(logLevelName(testCase.level), testCase.name);
    }
}

TEST(Settings, ParseLogLevel_OtherNames_Nullopt)
{
    // spdlog's own names for some levels, and a name in another case, are not the levels' names.
    for (const std::string_view name : {"", "Info", "warning", "err", "verbose"}) {
        SCOPED_TRACE(name);
        EXPECT_EQ(parseLogLevel(name), std::nullopt);
    }
}
