#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <thread>

#include <windows.h>

#include <gtest/gtest.h>

#include "app/system_collectors.h"
#include "domain/connectivity_status.h"
#include "domain/process_info.h"
#include "domain/system_snapshot.h"
#include "monitoring/health_evaluator.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/network_profile_collector.h"
#include "support/scheduler_harness.h"

using namespace sysmon::domain;
using sysmon::monitoring::evaluateHealth;
using sysmon::monitoring::HealthThresholds;
using sysmon::monitoring::SamplingScheduler;
using sysmon::platform::NetworkProfileCollector;
using sysmon::tests::waitForSnapshot;

namespace
{

// Returns true if the Network List Manager names the connected networks on
// this host. It may be unavailable, and then a snapshot has no network names.
bool networkListManagerAnswers()
{
    NetworkProfileCollector collector;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
    while (collector.readCount() == 0 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return collector.collect().has_value();
}

bool isValidPercent(float percent)
{
    return !std::isnan(percent) && percent >= 0.0f && percent <= 100.0f;
}

} // namespace

TEST(SchedulerPipelineIntegration, AppCollectors_SnapshotCarriesEveryField)
{
    const bool expectNetworkNames = networkListManagerAnswers();
    SamplingScheduler scheduler(std::chrono::milliseconds{50});
    scheduler.setSlowInterval(std::chrono::milliseconds{200});
    sysmon::app::registerSystemCollectors(scheduler);
    const auto started = std::chrono::steady_clock::now();

    // The first CPU sample only sets the baseline, and a tick in which some
    // core's counters did not advance has no per-core values. The slow
    // collectors' results arrive with the first slow refresh.
    const auto snapshot = waitForSnapshot(scheduler, [expectNetworkNames](const SystemSnapshot& candidate) {
        return candidate.cpu.has_value() && !candidate.cpu->coreUsagePercents.empty() &&
               candidate.processes.has_value() && (candidate.networkProfiles.has_value() || !expectNetworkNames);
    });

    ASSERT_TRUE(snapshot.has_value());
    EXPECT_GE(snapshot->timestamp, started);

    const auto logicalProcessors = static_cast<int>(::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    EXPECT_EQ(snapshot->cpu->coreCount, logicalProcessors);
    EXPECT_EQ(snapshot->cpu->coreUsagePercents.size(), static_cast<std::size_t>(logicalProcessors));
    EXPECT_TRUE(isValidPercent(snapshot->cpu->totalUsagePercent));

    ASSERT_TRUE(snapshot->memory.has_value());
    EXPECT_GT(snapshot->memory->totalBytes, 0u);
    EXPECT_GT(snapshot->memory->availableBytes, 0u);
    EXPECT_LE(snapshot->memory->availableBytes, snapshot->memory->totalBytes);
    EXPECT_TRUE(isValidPercent(snapshot->memory->usagePercent));

    // Any working Windows installation has a fixed drive.
    ASSERT_TRUE(snapshot->disks.has_value());
    EXPECT_FALSE(snapshot->disks->empty());
    for (const auto& disk : *snapshot->disks) {
        EXPECT_FALSE(disk.volumeName.empty());
        EXPECT_GT(disk.totalBytes, 0u);
        EXPECT_TRUE(isValidPercent(disk.usagePercent)) << disk.volumeName;
    }

    // The host may have no adapters besides loopback, which is left out.
    ASSERT_TRUE(snapshot->networks.has_value());
    for (const auto& adapter : *snapshot->networks) {
        EXPECT_FALSE(adapter.adapterName.empty());
    }

    EXPECT_TRUE(snapshot->connectivity.level >= ConnectivityLevel::Unknown &&
                snapshot->connectivity.level <= ConnectivityLevel::InternetAccess);
    if (snapshot->networkProfiles.has_value()) {
        for (const auto& profile : *snapshot->networkProfiles) {
            EXPECT_FALSE(profile.name.empty());
        }
    }

    ASSERT_TRUE(snapshot->uptime.has_value());
    EXPECT_GT(*snapshot->uptime, std::chrono::milliseconds{0});

    ASSERT_TRUE(snapshot->activity.has_value());
    EXPECT_GT(snapshot->activity->processCount, 0u);
    EXPECT_GE(snapshot->activity->threadCount, snapshot->activity->processCount);
    EXPECT_GT(snapshot->activity->handleCount, 0u);

    const auto& processes = *snapshot->processes;
    const auto self = std::ranges::find(processes, static_cast<uint32_t>(::GetCurrentProcessId()), &ProcessInfo::pid);
    ASSERT_NE(self, processes.end());
    EXPECT_EQ(self->detailsStatus, ProcessDetailsStatus::Available);

    EXPECT_EQ(snapshot->health, evaluateHealth(*snapshot, HealthThresholds{}));
}
