#include "domain/connectivity_status.h"
#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/memory_sample.h"
#include "domain/network_sample.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"

#include <gtest/gtest.h>

#include <chrono>

using namespace sysmon::domain;

// ---------------------------------------------------------------------------
// CpuSample
// ---------------------------------------------------------------------------

TEST(CpuSample_DefaultConstruction_ZeroValues, DefaultValues)
{
    CpuSample s;
    EXPECT_FLOAT_EQ(s.totalUsagePercent, 0.0f);
    EXPECT_TRUE(s.coreUsagePercents.empty());
    EXPECT_EQ(s.coreCount, 0);
}

TEST(CpuSample_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    CpuSample s{
        .totalUsagePercent = 42.5f,
        .coreUsagePercents = {30.0f, 55.0f},
        .coreCount         = 2,
    };
    EXPECT_FLOAT_EQ(s.totalUsagePercent, 42.5f);
    ASSERT_EQ(static_cast<int>(s.coreUsagePercents.size()), 2);
    EXPECT_FLOAT_EQ(s.coreUsagePercents[0], 30.0f);
    EXPECT_FLOAT_EQ(s.coreUsagePercents[1], 55.0f);
    EXPECT_EQ(s.coreCount, 2);
}

// ---------------------------------------------------------------------------
// MemorySample
// ---------------------------------------------------------------------------

TEST(MemorySample_DefaultConstruction_ZeroValues, DefaultValues)
{
    MemorySample s;
    EXPECT_EQ(s.totalBytes, 0u);
    EXPECT_EQ(s.availableBytes, 0u);
    EXPECT_FLOAT_EQ(s.usagePercent, 0.0f);
    EXPECT_EQ(s.commitLimit, 0u);
    EXPECT_EQ(s.commitCurrent, 0u);
}

TEST(MemorySample_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    MemorySample s{
        .totalBytes     = 16'000'000'000ULL,
        .availableBytes =  4'000'000'000ULL,
        .usagePercent   = 75.0f,
        .commitLimit    = 20'000'000'000ULL,
        .commitCurrent  =  8'000'000'000ULL,
    };
    EXPECT_EQ(s.totalBytes, 16'000'000'000ULL);
    EXPECT_EQ(s.availableBytes, 4'000'000'000ULL);
    EXPECT_FLOAT_EQ(s.usagePercent, 75.0f);
    EXPECT_EQ(s.commitLimit, 20'000'000'000ULL);
    EXPECT_EQ(s.commitCurrent, 8'000'000'000ULL);
}

// ---------------------------------------------------------------------------
// DiskSample
// ---------------------------------------------------------------------------

TEST(DiskSample_DefaultConstruction_ZeroValues, DefaultValues)
{
    DiskSample s;
    EXPECT_TRUE(s.volumeName.empty());
    EXPECT_EQ(s.totalBytes, 0u);
    EXPECT_EQ(s.freeBytes, 0u);
    EXPECT_FLOAT_EQ(s.usagePercent, 0.0f);
}

TEST(DiskSample_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    DiskSample s{
        .volumeName   = "C:\\",
        .totalBytes   = 500'000'000'000ULL,
        .freeBytes    = 200'000'000'000ULL,
        .usagePercent = 60.0f,
    };
    EXPECT_EQ(s.volumeName, "C:\\");
    EXPECT_EQ(s.totalBytes, 500'000'000'000ULL);
    EXPECT_EQ(s.freeBytes, 200'000'000'000ULL);
    EXPECT_FLOAT_EQ(s.usagePercent, 60.0f);
}

// ---------------------------------------------------------------------------
// NetworkSample
// ---------------------------------------------------------------------------

TEST(NetworkSample_DefaultConstruction_UnknownStatus, DefaultValues)
{
    NetworkSample s;
    EXPECT_TRUE(s.adapterName.empty());
    EXPECT_EQ(s.inBytesTotal, 0u);
    EXPECT_EQ(s.outBytesTotal, 0u);
    EXPECT_EQ(s.linkSpeedBps, 0u);
    EXPECT_EQ(s.operationalStatus, OperationalStatus::Unknown);
}

TEST(NetworkSample_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    NetworkSample s{
        .adapterName       = "Ethernet",
        .inBytesTotal      = 1'000'000ULL,
        .outBytesTotal     = 500'000ULL,
        .linkSpeedBps      = 1'000'000'000ULL,
        .operationalStatus = OperationalStatus::Up,
    };
    EXPECT_EQ(s.adapterName, "Ethernet");
    EXPECT_EQ(s.inBytesTotal, 1'000'000ULL);
    EXPECT_EQ(s.outBytesTotal, 500'000ULL);
    EXPECT_EQ(s.linkSpeedBps, 1'000'000'000ULL);
    EXPECT_EQ(s.operationalStatus, OperationalStatus::Up);
}

TEST(OperationalStatus_EnumValues_AreDistinct, EnumDistinctness)
{
    EXPECT_NE(OperationalStatus::Up,            OperationalStatus::Down);
    EXPECT_NE(OperationalStatus::Down,          OperationalStatus::Testing);
    EXPECT_NE(OperationalStatus::Testing,       OperationalStatus::Unknown);
    EXPECT_NE(OperationalStatus::Unknown,       OperationalStatus::Dormant);
    EXPECT_NE(OperationalStatus::Dormant,       OperationalStatus::NotPresent);
    EXPECT_NE(OperationalStatus::NotPresent,    OperationalStatus::LowerLayerDown);
}

// ---------------------------------------------------------------------------
// ConnectivityStatus
// ---------------------------------------------------------------------------

TEST(ConnectivityStatus_DefaultConstruction_NoneNotMetered, DefaultValues)
{
    ConnectivityStatus s;
    EXPECT_EQ(s.level, ConnectivityLevel::None);
    EXPECT_FALSE(s.isMetered);
}

TEST(ConnectivityStatus_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    ConnectivityStatus s{
        .level     = ConnectivityLevel::InternetAccess,
        .isMetered = true,
    };
    EXPECT_EQ(s.level, ConnectivityLevel::InternetAccess);
    EXPECT_TRUE(s.isMetered);
}

TEST(ConnectivityLevel_EnumValues_AreDistinct, EnumDistinctness)
{
    EXPECT_NE(ConnectivityLevel::None,
              ConnectivityLevel::LocalAccess);
    EXPECT_NE(ConnectivityLevel::LocalAccess,
              ConnectivityLevel::ConstrainedInternetAccess);
    EXPECT_NE(ConnectivityLevel::ConstrainedInternetAccess,
              ConnectivityLevel::InternetAccess);
}

// ---------------------------------------------------------------------------
// ProcessInfo
// ---------------------------------------------------------------------------

TEST(ProcessInfo_DefaultConstruction_ZeroAndFalse, DefaultValues)
{
    ProcessInfo p;
    EXPECT_EQ(p.pid, 0u);
    EXPECT_EQ(p.parentPid, 0u);
    EXPECT_TRUE(p.imageName.empty());
    EXPECT_FALSE(p.imagePath.has_value());
    EXPECT_FALSE(p.commandLine.has_value());
    EXPECT_EQ(p.cpuUserTimeMs, 0u);
    EXPECT_EQ(p.cpuKernelTimeMs, 0u);
    EXPECT_EQ(p.workingSetBytes, 0u);
    EXPECT_EQ(p.privateBytes, 0u);
    EXPECT_EQ(p.ioReadBytes, 0u);
    EXPECT_EQ(p.ioWriteBytes, 0u);
    EXPECT_EQ(p.threadCount, 0u);
    EXPECT_EQ(p.handleCount, 0u);
    EXPECT_FALSE(p.accessDenied);
}

TEST(ProcessInfo_DesignatedInit_FieldsRoundTrip, DesignatedInit)
{
    using namespace std::chrono;
    const auto now = system_clock::now();

    ProcessInfo p{
        .pid            = 1234,
        .creationTime   = now,
        .parentPid      = 1,
        .imageName      = "explorer.exe",
        .imagePath      = "C:\\Windows\\explorer.exe",
        .commandLine    = std::nullopt,
        .cpuUserTimeMs  = 100,
        .cpuKernelTimeMs = 50,
        .workingSetBytes = 64'000'000,
        .privateBytes    = 32'000'000,
        .ioReadBytes     = 1'000,
        .ioWriteBytes    = 500,
        .threadCount     = 24,
        .handleCount     = 512,
        .accessDenied    = false,
    };

    EXPECT_EQ(p.pid, 1234u);
    EXPECT_EQ(p.creationTime, now);
    EXPECT_EQ(p.parentPid, 1u);
    EXPECT_EQ(p.imageName, "explorer.exe");
    ASSERT_TRUE(p.imagePath.has_value());
    EXPECT_EQ(*p.imagePath, "C:\\Windows\\explorer.exe");
    EXPECT_FALSE(p.commandLine.has_value());
    EXPECT_EQ(p.cpuUserTimeMs, 100u);
    EXPECT_EQ(p.cpuKernelTimeMs, 50u);
    EXPECT_EQ(p.workingSetBytes, 64'000'000u);
    EXPECT_EQ(p.privateBytes, 32'000'000u);
    EXPECT_EQ(p.ioReadBytes, 1'000u);
    EXPECT_EQ(p.ioWriteBytes, 500u);
    EXPECT_EQ(p.threadCount, 24u);
    EXPECT_EQ(p.handleCount, 512u);
    EXPECT_FALSE(p.accessDenied);
}

// Two ProcessInfo values with the same PID but different creation times are
// distinct identities. The test encodes the design decision: callers must
// never use PID alone as a stable key.
TEST(ProcessInfo_SamePidDifferentCreationTime_DifferentIdentity, PidReuse)
{
    using namespace std::chrono;
    const auto t1 = system_clock::time_point{seconds{1000}};
    const auto t2 = system_clock::time_point{seconds{2000}};

    ProcessInfo a{.pid = 42, .creationTime = t1, .imageName = "old.exe"};
    ProcessInfo b{.pid = 42, .creationTime = t2, .imageName = "new.exe"};

    // Same PID — but different identity
    EXPECT_EQ(a.pid, b.pid);
    EXPECT_NE(a.creationTime, b.creationTime);
    EXPECT_NE(a.imageName, b.imageName);
}

// ---------------------------------------------------------------------------
// SystemSnapshot
// ---------------------------------------------------------------------------

TEST(SystemSnapshot_DefaultConstruction_EmptyCollections, DefaultValues)
{
    SystemSnapshot snap;
    EXPECT_TRUE(snap.disks.empty());
    EXPECT_TRUE(snap.networks.empty());
    EXPECT_TRUE(snap.processes.empty());
    EXPECT_EQ(snap.connectivity.level, ConnectivityLevel::None);
}

TEST(SystemSnapshot_DesignatedInit_SubSamplesStoredCorrectly, DesignatedInit)
{
    using namespace std::chrono;
    const auto ts = steady_clock::now();

    SystemSnapshot snap{
        .timestamp    = ts,
        .cpu          = {.totalUsagePercent = 10.0f, .coreCount = 4},
        .memory       = {.totalBytes = 8'000'000'000ULL, .usagePercent = 50.0f},
        .disks        = {DiskSample{.volumeName = "C:\\", .usagePercent = 70.0f}},
        .networks     = {NetworkSample{.adapterName = "Wi-Fi",
                                       .operationalStatus = OperationalStatus::Up}},
        .connectivity = {.level = ConnectivityLevel::InternetAccess},
        .processes    = {ProcessInfo{.pid = 4, .imageName = "System"}},
    };

    EXPECT_EQ(snap.timestamp, ts);
    EXPECT_FLOAT_EQ(snap.cpu.totalUsagePercent, 10.0f);
    EXPECT_EQ(snap.cpu.coreCount, 4);
    EXPECT_EQ(snap.memory.totalBytes, 8'000'000'000ULL);
    ASSERT_EQ(snap.disks.size(), 1u);
    EXPECT_EQ(snap.disks[0].volumeName, "C:\\");
    ASSERT_EQ(snap.networks.size(), 1u);
    EXPECT_EQ(snap.networks[0].adapterName, "Wi-Fi");
    EXPECT_EQ(snap.connectivity.level, ConnectivityLevel::InternetAccess);
    ASSERT_EQ(snap.processes.size(), 1u);
    EXPECT_EQ(snap.processes[0].pid, 4u);
}

