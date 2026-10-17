#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "domain/system_snapshot.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/network_collector.h"
#include "support/scheduler_harness.h"

using namespace sysmon::domain;
using namespace sysmon::monitoring;
using namespace sysmon::platform;
using sysmon::tests::waitForSnapshot;

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

        // Addresses come from the same GetAdaptersAddresses entry that marks
        // the adapter as bound to IP.
        if (!sample.ipAddresses.empty()) {
            EXPECT_TRUE(sample.hasIpInterface) << sample.adapterName;
        }

        // A link-local IPv6 address carries its zone index, as ipconfig shows it.
        for (const auto& address : sample.ipAddresses) {
            if (address.isLinkLocal && address.text.find(':') != std::string::npos) {
                EXPECT_NE(address.text.find('%'), std::string::npos) << address.text;
            }
            // Windows generates temporary addresses for IPv6 only.
            if (address.isTemporary) {
                EXPECT_NE(address.text.find(':'), std::string::npos) << address.text;
            }
        }
    }

    // Interface IDs identify adapters, so no two may share one, even where names repeat.
    std::vector<uint64_t> ids;
    for (const auto& sample : *samples) {
        ids.push_back(sample.interfaceId);
    }
    std::ranges::sort(ids);
    EXPECT_EQ(std::ranges::adjacent_find(ids), ids.end());
}

TEST(NetworkCollectorIntegration, RealHostSampling_ReportsNoPlaceholderDnsServers)
{
    NetworkCollector collector;
    const auto samples = collector.collect();
    ASSERT_TRUE(samples.has_value());

    // Windows lists these as the DNS servers of an adapter that has none configured.
    const std::vector<std::string> placeholders = {"fec0:0:0:ffff::1", "fec0:0:0:ffff::2", "fec0:0:0:ffff::3"};
    for (const auto& sample : *samples) {
        for (const auto& server : sample.dnsServers) {
            EXPECT_EQ(std::ranges::find(placeholders, server), placeholders.end())
                << sample.adapterName << ": " << server;
        }
    }
}

TEST(NetworkCollectorIntegration, RealHostSampling_WanMiniportsHidden)
{
    NetworkCollector collector;
    const auto samples = collector.collect();
    ASSERT_TRUE(samples.has_value());

    // Their drivers mark the WAN Miniports hidden (NCF_HIDDEN) on every
    // Windows install that has them.
    for (const auto& sample : *samples) {
        if (sample.description.starts_with("WAN Miniport")) {
            EXPECT_TRUE(sample.isHidden) << sample.description;
        }
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

    // The second snapshot has a baseline for each adapter's rates.
    int snapshotCount = 0;
    const auto snapshot =
        waitForSnapshot(scheduler, [&snapshotCount](const SystemSnapshot&) { return ++snapshotCount == 2; });

    ASSERT_TRUE(snapshot.has_value());
    ASSERT_TRUE(snapshot->networks.has_value());
    for (const auto& adapter : *snapshot->networks) {
        EXPECT_FALSE(adapter.adapterName.empty());
        EXPECT_GE(adapter.inBytesTotal, 0u);
        EXPECT_GE(adapter.outBytesTotal, 0u);
    }
}
