#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>

#include "domain/system_snapshot.h"
#include "ui/adapter_card.h"
#include "ui/network_view.h"

using namespace sysmon::domain;
using namespace sysmon::ui;

namespace
{

NetworkSample adapter(uint64_t interfaceId, std::string name, OperationalStatus status, bool isHardware = true,
                      bool hasIpInterface = true)
{
    return NetworkSample{
        .interfaceId = interfaceId,
        .adapterName = "{" + name + "}",
        .friendlyName = std::move(name),
        .inBytesPerSec = 2048,
        .outBytesPerSec = 1024,
        .linkSpeedBps = 1'000'000'000,
        .operationalStatus = status,
        .isHardwareInterface = isHardware,
        .hasIpInterface = hasIpInterface,
    };
}

SystemSnapshot snapshotWith(std::optional<std::vector<NetworkSample>> networks)
{
    SystemSnapshot snapshot;
    snapshot.networks = std::move(networks);
    return snapshot;
}

// A view that counts as visible without opening a window on screen.
class ShownNetworkView : public NetworkView
{
public:
    ShownNetworkView()
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        show();
    }
};

} // namespace

TEST(NetworkView, Construction_PlaceholderBeforeFirstSnapshot)
{
    ShownNetworkView view;

    EXPECT_TRUE(view.listedInterfaceIds().empty());
    EXPECT_EQ(view.placeholderText(), "--");
    EXPECT_FALSE(view.showsAllAdapters());
}

TEST(NetworkView, Snapshot_ListsAdaptersWithCardsTitledByDisplayName)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Wi-Fi", OperationalStatus::Down),
        adapter(2, "Ethernet", OperationalStatus::Up),
    }));

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{2, 1}));
    EXPECT_EQ(view.adapterCard(0)->title(), "Ethernet");
    EXPECT_EQ(view.adapterCard(1)->title(), "Wi-Fi");
}

TEST(NetworkView, UnboundVirtualAdapters_ListedOnlyWithShowAll)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, false, false),
    }));
    ASSERT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1}));
    AdapterCard* ethernetCard = view.adapterCard(0);

    view.setShowAllAdapters(true);

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1, 2}));
    EXPECT_EQ(view.adapterCard(0), ethernetCard);
    EXPECT_EQ(view.adapterCard(1)->title(), "WAN Miniport (IP)");

    view.setShowAllAdapters(false);

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1}));
}

TEST(NetworkView, AdapterComesUp_MovesToTheTopAndKeepsItsCard)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "Wi-Fi", OperationalStatus::Down),
    }));
    AdapterCard* wifiCard = view.adapterCard(1);

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Down),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
    }));

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{2, 1}));
    EXPECT_EQ(view.adapterCard(0), wifiCard);
}

TEST(NetworkView, AdapterRenamed_KeepsCardAndUpdatesTitle)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));
    AdapterCard* card = view.adapterCard(0);

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Office LAN", OperationalStatus::Up)}));

    EXPECT_EQ(view.adapterCard(0), card);
    EXPECT_EQ(card->title(), "Office LAN");
}

TEST(NetworkView, AdapterRemoved_CardDropped)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
    }));

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(2, "Wi-Fi", OperationalStatus::Up)}));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{2}));
    EXPECT_EQ(view.findChildren<AdapterCard*>().size(), 1);
}

TEST(NetworkView, NoAdapters_ShowsPlaceholder)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{}));

    EXPECT_TRUE(view.listedInterfaceIds().empty());
    EXPECT_EQ(view.placeholderText(), "No network adapters");
}

TEST(NetworkView, ReadFailsBeforeAnyAdapters_ShowsNotAvailable)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWith(std::nullopt));

    EXPECT_EQ(view.placeholderText(), "N/A");
}

TEST(NetworkView, ReadFails_KeepsCardsOfTheLastRead)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    view.updateSnapshot(snapshotWith(std::nullopt));

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1}));
}

TEST(NetworkView, Hidden_UpdatesCardsOnlyWhenShown)
{
    NetworkView view;
    view.setAttribute(Qt::WA_DontShowOnScreen);

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    EXPECT_TRUE(view.listedInterfaceIds().empty());

    view.show();

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1}));
}
