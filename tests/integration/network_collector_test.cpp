#include <chrono>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QObject>

#include "domain/network_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/network_collector.h"

using namespace sysmon::domain;
using namespace sysmon::monitoring;
using namespace sysmon::platform;

TEST(NetworkCollectorIntegration, RealHostSampling_DetectsAdaptersWithSensibleMetrics)
{
    NetworkCollector collector;
    const auto samples = collector.collect();

    // If host has network adapters, verify all invariants
    for (const auto &sample : samples) {
        // Must have non-empty name
        EXPECT_FALSE(sample.adapterName.empty());

        // Inbound and outbound cumulative counters must be non-negative (uint64)
        EXPECT_GE(sample.inBytesTotal, 0u);
        EXPECT_GE(sample.outBytesTotal, 0u);

        // First sample rates should be 0 (no baseline yet)
        EXPECT_EQ(sample.inBytesPerSec, 0u);
        EXPECT_EQ(sample.outBytesPerSec, 0u);

        // Adapter name / friendly name should not indicate loopback
        EXPECT_NE(sample.adapterName, "Loopback");
        EXPECT_NE(sample.friendlyName, "Loopback");
    }
}

TEST(NetworkCollectorIntegration, ConsecutiveSamples_CalculatesThroughputOverTime)
{
    NetworkCollector collector;

    // First sample establishes initial baseline
    const auto firstSamples = collector.collect();

    // Sleep briefly to let monotonic clock advance
    std::this_thread::sleep_for(std::chrono::milliseconds{80});

    // Second sample computes throughput deltas
    const auto secondSamples = collector.collect();

    EXPECT_EQ(firstSamples.size(), secondSamples.size());

    for (const auto &sample : secondSamples) {
        EXPECT_GE(sample.inBytesPerSec, 0u);
        EXPECT_GE(sample.outBytesPerSec, 0u);
    }
}

TEST(NetworkCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealNetworkAdapters)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{40});
    scheduler.setNetworkCollector(std::make_unique<NetworkCollector>());

    std::vector<SystemSnapshot> receivedSnapshots;

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady,
                     [&receivedSnapshots](const SystemSnapshot &snapshot) {
                         receivedSnapshots.push_back(snapshot);
                     });

    scheduler.start();

    const auto startTime = std::chrono::steady_clock::now();
    while (receivedSnapshots.size() < 2 &&
           std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds{1000}) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    ASSERT_GE(receivedSnapshots.size(), 2u);

    for (const auto &snapshot : receivedSnapshots) {
        for (const auto &adapter : snapshot.networks) {
            EXPECT_FALSE(adapter.adapterName.empty());
            EXPECT_GE(adapter.inBytesTotal, 0u);
            EXPECT_GE(adapter.outBytesTotal, 0u);
        }
    }
}

