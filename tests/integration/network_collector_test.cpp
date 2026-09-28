#include <algorithm>
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
    ASSERT_TRUE(samples.has_value());

    // If host has network adapters, verify all invariants
    for (const auto& sample : *samples) {
        // Must have non-empty name
        EXPECT_FALSE(sample.adapterName.empty());

        // Inbound and outbound cumulative counters must be non-negative (uint64)
        EXPECT_GE(sample.inBytesTotal, 0u);
        EXPECT_GE(sample.outBytesTotal, 0u);

        // No baseline yet on the first sample, so no rate can be reported
        EXPECT_FALSE(sample.inBytesPerSec.has_value());
        EXPECT_FALSE(sample.outBytesPerSec.has_value());

        // Adapter name / friendly name should not indicate loopback
        EXPECT_NE(sample.adapterName, "Loopback");
        EXPECT_NE(sample.friendlyName, "Loopback");
    }
}

TEST(NetworkCollectorIntegration, ConsecutiveSamples_CalculatesThroughputOverTime)
{
    NetworkCollector collector;

    // First sample establishes initial baseline
    const auto firstResult = collector.collect();

    // Sleep briefly to let monotonic clock advance
    std::this_thread::sleep_for(std::chrono::milliseconds{80});

    // Second sample computes throughput deltas
    const auto secondResult = collector.collect();
    ASSERT_TRUE(firstResult.has_value());
    ASSERT_TRUE(secondResult.has_value());
    const auto& firstSamples = *firstResult;
    const auto& secondSamples = *secondResult;

    EXPECT_EQ(firstSamples.size(), secondSamples.size());

    // Adapters present in both samples have a baseline, so both rates must be reported.
    // (An adapter that appeared in between legitimately has no rate yet.)
    for (const auto& sample : secondSamples) {
        const bool seenBefore = std::ranges::any_of(
            firstSamples, [&sample](const NetworkSample& first) { return first.adapterName == sample.adapterName; });
        if (seenBefore) {
            EXPECT_TRUE(sample.inBytesPerSec.has_value()) << sample.adapterName;
            EXPECT_TRUE(sample.outBytesPerSec.has_value()) << sample.adapterName;
        }
    }
}

TEST(NetworkCollectorIntegration, ConsecutiveSamples_KeepAdapterDetailsBetweenRefreshes)
{
    NetworkCollector collector;

    // The first collect() reads adapter details; the second is served from the cache.
    const auto firstResult = collector.collect();
    const auto secondResult = collector.collect();
    ASSERT_TRUE(firstResult.has_value());
    ASSERT_TRUE(secondResult.has_value());
    const auto& firstSamples = *firstResult;
    const auto& secondSamples = *secondResult;

    for (const auto& sample : secondSamples) {
        const auto first = std::ranges::find_if(firstSamples, [&sample](const NetworkSample& candidate) {
            return candidate.adapterName == sample.adapterName;
        });
        if (first != firstSamples.end()) {
            EXPECT_EQ(sample.friendlyName, first->friendlyName) << sample.adapterName;
            EXPECT_EQ(sample.description, first->description) << sample.adapterName;
        }
    }
}

TEST(NetworkCollectorIntegration, RepeatedConstructionAndDestruction_CancelsNotificationsCleanly)
{
    for (int i = 0; i < 20; ++i) {
        NetworkCollector collector;
        static_cast<void>(collector.collect());
    }
    SUCCEED();
}

TEST(NetworkCollectorIntegration, SchedulerPipeline_EmitsSnapshotsWithRealNetworkAdapters)
{
    SamplingScheduler scheduler(std::chrono::milliseconds{40});
    scheduler.setNetworkCollector(std::make_unique<NetworkCollector>());

    std::vector<SystemSnapshot> receivedSnapshots;

    QObject::connect(&scheduler, &SamplingScheduler::snapshotReady,
                     [&receivedSnapshots](const SystemSnapshot& snapshot) { receivedSnapshots.push_back(snapshot); });

    scheduler.start();

    const auto startTime = std::chrono::steady_clock::now();
    while (receivedSnapshots.size() < 2 &&
           std::chrono::steady_clock::now() - startTime < std::chrono::milliseconds{1000}) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    scheduler.stop();

    ASSERT_GE(receivedSnapshots.size(), 2u);

    for (const auto& snapshot : receivedSnapshots) {
        ASSERT_TRUE(snapshot.networks.has_value());
        for (const auto& adapter : *snapshot.networks) {
            EXPECT_FALSE(adapter.adapterName.empty());
            EXPECT_GE(adapter.inBytesTotal, 0u);
            EXPECT_GE(adapter.outBytesTotal, 0u);
        }
    }
}
