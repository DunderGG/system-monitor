#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "domain/network_sample.h"
#include "ui/network_formatting.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

NetworkSample adapter(uint64_t interfaceId, std::string name, OperationalStatus status, bool isHardware = true,
                      bool hasIpInterface = true)
{
    return NetworkSample{
        .interfaceId = interfaceId,
        .adapterName = name,
        .friendlyName = std::move(name),
        .operationalStatus = status,
        .isHardwareInterface = isHardware,
        .hasIpInterface = hasIpInterface,
    };
}

std::vector<uint64_t> ids(const std::vector<const NetworkSample*>& adapters)
{
    std::vector<uint64_t> result;
    for (const auto* sample : adapters) {
        result.push_back(sample->interfaceId);
    }
    return result;
}

} // namespace

TEST(NetworkFormatting, IsListedByDefault_HardwareOrIpBound)
{
    EXPECT_TRUE(isListedByDefault(adapter(1, "Ethernet", OperationalStatus::Down, true, false)));
    EXPECT_TRUE(isListedByDefault(adapter(2, "Mullvad", OperationalStatus::Up, false, true)));
    EXPECT_FALSE(isListedByDefault(adapter(3, "WAN Miniport (IP)", OperationalStatus::Up, false, false)));
}

TEST(NetworkFormatting, ListedAdapters_HidesUnboundVirtualAdaptersUnlessShowAll)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, false, false),
        adapter(3, "Teredo Tunneling Pseudo-Interface", OperationalStatus::NotPresent, false, false),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{1}));
    EXPECT_EQ(ids(listedAdapters(networks, true)), (std::vector<uint64_t>{1, 2, 3}));
}

TEST(NetworkFormatting, ListedAdapters_DownAdaptersListedAfterActiveOnes)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "Wi-Fi", OperationalStatus::Down),
        adapter(2, "Ethernet", OperationalStatus::Up),
        adapter(3, "Bluetooth Network Connection", OperationalStatus::LowerLayerDown, false, true),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{2, 1, 3}));
}

TEST(NetworkFormatting, ListedAdapters_UpThenHardwareThenNameThenId)
{
    const std::vector<NetworkSample> networks{
        adapter(1, "mullvad", OperationalStatus::Up, false, true),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
        adapter(3, "Ethernet", OperationalStatus::Up),
        adapter(5, "Local Area Connection* 1", OperationalStatus::Down, false, true),
        adapter(4, "Local Area Connection* 1", OperationalStatus::Down, false, true),
        adapter(6, "Bluetooth", OperationalStatus::Up, false, true),
    };

    EXPECT_EQ(ids(listedAdapters(networks, false)), (std::vector<uint64_t>{3, 2, 6, 1, 4, 5}));
}

TEST(NetworkFormatting, ListedAdapters_NoAdapters_Empty)
{
    EXPECT_TRUE(listedAdapters({}, false).empty());
}
