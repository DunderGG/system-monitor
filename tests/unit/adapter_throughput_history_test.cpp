#include <cstdint>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "support/network_samples.h"
#include "ui/adapter_throughput_history.h"

using namespace sysmon::domain;
using namespace sysmon::ui;
using sysmon::tests::adapter;
using sysmon::tests::withRates;

namespace
{

std::vector<std::optional<float>> samplesOf(const SampleHistory& history)
{
    const auto samples = history.samples();
    return {samples.begin(), samples.end()};
}

} // namespace

TEST(AdapterThroughputHistory, Record_AppendsReceiveAndSendPerAdapter)
{
    AdapterThroughputHistory history(60);

    history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), 100, 10),
                                              withRates(adapter(2, "Wi-Fi"), 200, 20)});
    history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), 300, 30),
                                              withRates(adapter(2, "Wi-Fi"), 400, 40)});

    ASSERT_NE(history.find(1), nullptr);
    EXPECT_EQ(samplesOf(history.find(1)->receive), (std::vector<std::optional<float>>{100.0f, 300.0f}));
    EXPECT_EQ(samplesOf(history.find(1)->send), (std::vector<std::optional<float>>{10.0f, 30.0f}));
    ASSERT_NE(history.find(2), nullptr);
    EXPECT_EQ(samplesOf(history.find(2)->receive), (std::vector<std::optional<float>>{200.0f, 400.0f}));
    EXPECT_EQ(history.find(3), nullptr);
}

TEST(AdapterThroughputHistory, Record_NewAdapterWithoutRates_KeptWithGap)
{
    AdapterThroughputHistory history(60);

    history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), std::nullopt, std::nullopt)});

    ASSERT_NE(history.find(1), nullptr);
    EXPECT_EQ(samplesOf(history.find(1)->receive), (std::vector<std::optional<float>>{std::nullopt}));
}

TEST(AdapterThroughputHistory, Record_ReadFails_GapForEveryAdapter)
{
    AdapterThroughputHistory history(60);
    history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), 100, 10)});

    history.record(std::nullopt);

    ASSERT_NE(history.find(1), nullptr);
    EXPECT_EQ(samplesOf(history.find(1)->receive), (std::vector<std::optional<float>>{100.0f, std::nullopt}));
    EXPECT_EQ(samplesOf(history.find(1)->send), (std::vector<std::optional<float>>{10.0f, std::nullopt}));
}

TEST(AdapterThroughputHistory, Record_AdapterGone_GapsUntilNoValueLeftThenDropped)
{
    AdapterThroughputHistory history(2);
    history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), 100, 10)});

    history.record(std::vector<NetworkSample>{});

    ASSERT_NE(history.find(1), nullptr);
    EXPECT_EQ(history.adapterCount(), 1u);

    history.record(std::vector<NetworkSample>{});

    EXPECT_EQ(history.find(1), nullptr);
    EXPECT_EQ(history.adapterCount(), 0u);
}

TEST(AdapterThroughputHistory, Record_KeepsCapacitySamples)
{
    AdapterThroughputHistory history(2);

    for (uint64_t rate = 1; rate <= 3; ++rate) {
        history.record(std::vector<NetworkSample>{withRates(adapter(1, "Ethernet"), rate, rate)});
    }

    ASSERT_NE(history.find(1), nullptr);
    EXPECT_EQ(samplesOf(history.find(1)->receive), (std::vector<std::optional<float>>{2.0f, 3.0f}));
}
