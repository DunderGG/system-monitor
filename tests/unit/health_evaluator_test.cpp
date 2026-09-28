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
    snapshot.disks = std::vector<DiskSample>{DiskSample{.volumeName = "C:\\", .totalBytes = 1000, .usagePercent = 50.0f}};
    snapshot.connectivity.level = ConnectivityLevel::InternetAccess;
    return snapshot;
}

} // namespace

// ---------------------------------------------------------------------------
// evaluateUsage
// ---------------------------------------------------------------------------

TEST(HealthEvaluator, EvaluateUsage_NoData_ReturnsUnknown)
{
    EXPECT_EQ(evaluateUsage(std::nullopt, kThresholds), HealthLevel::Unknown);
}

TEST(HealthEvaluator, EvaluateUsage_BelowWarning_ReturnsHealthy)
{
    EXPECT_EQ(evaluateUsage(0.0f, kThresholds), HealthLevel::Healthy);
    EXPECT_EQ(evaluateUsage(50.0f, kThresholds), HealthLevel::Healthy);
}

TEST(HealthEvaluator, EvaluateUsage_ExactlyAtWarning_ReturnsHealthy)
{
    EXPECT_EQ(evaluateUsage(90.0f, kThresholds), HealthLevel::Healthy);
}

TEST(HealthEvaluator, EvaluateUsage_AboveWarning_ReturnsWarning)
{
    EXPECT_EQ(evaluateUsage(90.5f, kThresholds), HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateUsage_ExactlyAtCritical_ReturnsWarning)
{
    EXPECT_EQ(evaluateUsage(95.0f, kThresholds), HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateUsage_AboveCritical_ReturnsCritical)
{
    EXPECT_EQ(evaluateUsage(95.5f, kThresholds), HealthLevel::Critical);
    EXPECT_EQ(evaluateUsage(100.0f, kThresholds), HealthLevel::Critical);
}

// ---------------------------------------------------------------------------
// evaluateConnectivity
// ---------------------------------------------------------------------------

TEST(HealthEvaluator, EvaluateConnectivity_InternetAccess_ReturnsHealthy)
{
    EXPECT_EQ(evaluateConnectivity(ConnectivityLevel::InternetAccess), HealthLevel::Healthy);
}

TEST(HealthEvaluator, EvaluateConnectivity_DegradedLevels_ReturnWarning)
{
    EXPECT_EQ(evaluateConnectivity(ConnectivityLevel::None), HealthLevel::Warning);
    EXPECT_EQ(evaluateConnectivity(ConnectivityLevel::LocalAccess), HealthLevel::Warning);
    EXPECT_EQ(evaluateConnectivity(ConnectivityLevel::ConstrainedInternetAccess), HealthLevel::Warning);
}

TEST(HealthEvaluator, EvaluateConnectivity_Unknown_ReturnsUnknown)
{
    EXPECT_EQ(evaluateConnectivity(ConnectivityLevel::Unknown), HealthLevel::Unknown);
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
