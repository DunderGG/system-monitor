#include <array>
#include <optional>

#include <gtest/gtest.h>

#include "domain/connectivity_status.h"
#include "domain/cpu_sample.h"
#include "domain/disk_sample.h"
#include "domain/health_status.h"
#include "domain/memory_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/health_evaluator.h"

using namespace sysmon::domain;
using namespace sysmon::monitoring;

namespace
{

constexpr UsageThresholds kThresholds{.warningPercent = 90.0f, .criticalPercent = 95.0f};

// A snapshot in which every component evaluates to Healthy with default thresholds.
SystemSnapshot healthySnapshot()
{
    SystemSnapshot snapshot;
    snapshot.cpu = CpuSample{.totalUsagePercent = 20.0f, .coreCount = 4};
    snapshot.memory = MemorySample{.totalBytes = 16'000'000'000ULL, .usagePercent = 40.0f};
    snapshot.disks =
        std::vector<DiskSample>{DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .usagePercent = 50.0f}};
    snapshot.connectivity.level = ConnectivityLevel::InternetAccess;
    return snapshot;
}

} // namespace

// ---------------------------------------------------------------------------
// evaluateUsage
// ---------------------------------------------------------------------------

TEST(HealthEvaluator, EvaluateUsage_EachLevelStartsAboveItsThreshold)
{
    struct Case
    {
        const char* name;
        std::optional<float> usagePercent;
        HealthLevel expected;
    };
    const std::array cases{
        Case{"no data", std::nullopt, HealthLevel::Unknown},  Case{"idle", 0.0f, HealthLevel::Healthy},
        Case{"below warning", 50.0f, HealthLevel::Healthy},   Case{"exactly at warning", 90.0f, HealthLevel::Healthy},
        Case{"above warning", 90.5f, HealthLevel::Warning},   Case{"exactly at critical", 95.0f, HealthLevel::Warning},
        Case{"above critical", 95.5f, HealthLevel::Critical}, Case{"full", 100.0f, HealthLevel::Critical},
    };

    for (const auto& [name, usagePercent, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(evaluateUsage(usagePercent, kThresholds), expected);
    }
}

// ---------------------------------------------------------------------------
// evaluateConnectivity
// ---------------------------------------------------------------------------

TEST(HealthEvaluator, EvaluateConnectivity_InternetHealthyDegradedWarningUnknownUnknown)
{
    struct Case
    {
        const char* name;
        ConnectivityLevel level;
        HealthLevel expected;
    };
    const std::array cases{
        Case{"InternetAccess", ConnectivityLevel::InternetAccess, HealthLevel::Healthy},
        Case{"ConstrainedInternetAccess", ConnectivityLevel::ConstrainedInternetAccess, HealthLevel::Warning},
        Case{"LocalAccess", ConnectivityLevel::LocalAccess, HealthLevel::Warning},
        Case{"None", ConnectivityLevel::None, HealthLevel::Warning},
        Case{"Unknown", ConnectivityLevel::Unknown, HealthLevel::Unknown},
    };

    for (const auto& [name, level, expected] : cases) {
        SCOPED_TRACE(name);
        EXPECT_EQ(evaluateConnectivity(level), expected);
    }
}

// ---------------------------------------------------------------------------
// evaluateHealth
// ---------------------------------------------------------------------------

TEST(HealthEvaluator, EvaluateHealth_EmptySnapshot_AllUnknown)
{
    const auto health = evaluateHealth(SystemSnapshot{}, HealthThresholds{});

    EXPECT_EQ(health, SystemHealth{});
    EXPECT_EQ(health.overall, HealthLevel::Unknown);
}

TEST(HealthEvaluator, EvaluateHealth_AllWithinLimits_OverallHealthy)
{
    const auto health = evaluateHealth(healthySnapshot(), HealthThresholds{});

    EXPECT_EQ(health.cpu, HealthLevel::Healthy);
    EXPECT_EQ(health.memory, HealthLevel::Healthy);
    EXPECT_EQ(health.disk, HealthLevel::Healthy);
    EXPECT_EQ(health.network, HealthLevel::Healthy);
    EXPECT_EQ(health.overall, HealthLevel::Healthy);
}

TEST(HealthEvaluator, EvaluateHealth_MemoryAbove90Percent_MemoryAndOverallWarning)
{
    auto snapshot = healthySnapshot();
    snapshot.memory->usagePercent = 92.0f;

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.memory, HealthLevel::Warning);
    EXPECT_EQ(health.overall, HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateHealth_CpuFullLoadBurst_WarningNotCritical)
{
    auto snapshot = healthySnapshot();
    snapshot.cpu->totalUsagePercent = 97.0f;

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.cpu, HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateHealth_DiskUsesWorstVolume)
{
    auto snapshot = healthySnapshot();
    snapshot.disks = std::vector<DiskSample>{
        DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .usagePercent = 50.0f},
        DiskSample{.volumeName = "D:\\", .totalBytes = 1000, .usagePercent = 99.0f},
        DiskSample{.volumeName = "E:\\", .totalBytes = 1000, .usagePercent = 92.0f},
    };

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.disk, HealthLevel::Critical);
    EXPECT_EQ(health.overall, HealthLevel::Critical);
}

TEST(HealthEvaluator, EvaluateHealth_NoDiskVolumes_DiskUnknown)
{
    auto snapshot = healthySnapshot();
    snapshot.disks = std::vector<DiskSample>{};

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.disk, HealthLevel::Unknown);
}

TEST(HealthEvaluator, EvaluateHealth_NoDiskData_DiskUnknown)
{
    auto snapshot = healthySnapshot();
    snapshot.disks.reset();

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.disk, HealthLevel::Unknown);
    EXPECT_EQ(health.overall, HealthLevel::Unknown);
}

TEST(HealthEvaluator, EvaluateHealth_OneComponentUnknownRestHealthy_OverallUnknown)
{
    auto snapshot = healthySnapshot();
    snapshot.cpu.reset();

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.cpu, HealthLevel::Unknown);
    EXPECT_EQ(health.overall, HealthLevel::Unknown);
}

TEST(HealthEvaluator, EvaluateHealth_UnknownAndWarning_OverallWarning)
{
    auto snapshot = healthySnapshot();
    snapshot.cpu.reset();
    snapshot.connectivity.level = ConnectivityLevel::None;

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.overall, HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateHealth_CriticalAndWarning_OverallCritical)
{
    auto snapshot = healthySnapshot();
    snapshot.memory->usagePercent = 99.0f;
    snapshot.connectivity.level = ConnectivityLevel::LocalAccess;

    const auto health = evaluateHealth(snapshot, HealthThresholds{});

    EXPECT_EQ(health.memory, HealthLevel::Critical);
    EXPECT_EQ(health.network, HealthLevel::Warning);
    EXPECT_EQ(health.overall, HealthLevel::Critical);
}

TEST(HealthEvaluator, EvaluateHealth_CustomThresholds_Applied)
{
    auto snapshot = healthySnapshot();
    const HealthThresholds thresholds{
        .memory = UsageThresholds{.warningPercent = 30.0f, .criticalPercent = 35.0f},
    };

    const auto health = evaluateHealth(snapshot, thresholds);

    EXPECT_EQ(health.memory, HealthLevel::Critical);
}
