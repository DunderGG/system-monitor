#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>

#include "domain/system_snapshot.h"
#include "ui/adapter_card.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/network_view.h"
#include "ui/readout_grid.h"

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

TEST(NetworkView, Card_ShowsStatusDescriptionAndDetails)
{
    ShownNetworkView view;
    NetworkSample wifi = adapter(1, "Wi-Fi", OperationalStatus::Up);
    wifi.description = "Intel(R) Wi-Fi 6 AX201 160MHz";
    wifi.type = AdapterType::WiFi;
    wifi.ipAddresses = {"192.168.1.23", "fe80::1c2b"};
    wifi.dnsServers = {"192.168.1.1"};

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{wifi}));

    const AdapterCard* card = view.adapterCard(0);
    EXPECT_EQ(card->title(), "Wi-Fi");
    EXPECT_EQ(card->description(), "Intel(R) Wi-Fi 6 AX201 160MHz");
    EXPECT_EQ(card->statusText(), "Up");
    EXPECT_EQ(card->readoutGrid()->value("Type"), "Wi-Fi");
    EXPECT_EQ(card->readoutGrid()->value("Link speed"), "1 Gbps");
    EXPECT_EQ(card->readoutGrid()->value("IPv4 address"), "192.168.1.23");
    EXPECT_EQ(card->readoutGrid()->value("IPv6 address"), "fe80::1c2b");
    EXPECT_EQ(card->readoutGrid()->value("DNS servers"), "192.168.1.1");
}

TEST(NetworkView, Card_DescriptionSameAsTitle_Hidden)
{
    ShownNetworkView view;
    NetworkSample unnamed = adapter(1, "", OperationalStatus::Down);
    unnamed.description = "Realtek Gaming 2.5GbE Family Controller";

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{unnamed}));

    EXPECT_EQ(view.adapterCard(0)->title(), "Realtek Gaming 2.5GbE Family Controller");
    EXPECT_TRUE(view.adapterCard(0)->description().isEmpty());
    EXPECT_EQ(view.adapterCard(0)->statusText(), "Down");
    EXPECT_EQ(view.adapterCard(0)->readoutGrid()->value("Link speed"), "Not connected");
}

TEST(NetworkView, ReadFails_CardDetailsNotAvailableThenRecover)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    view.updateSnapshot(snapshotWith(std::nullopt));

    const AdapterCard* card = view.adapterCard(0);
    EXPECT_EQ(card->title(), "Ethernet");
    EXPECT_EQ(card->statusText(), "N/A");
    EXPECT_EQ(card->readoutGrid()->value("Link speed"), "N/A");

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    EXPECT_EQ(card->statusText(), "Up");
    EXPECT_EQ(card->readoutGrid()->value("Link speed"), "1 Gbps");
}

TEST(NetworkView, Card_ChartsReceiveAndSend)
{
    ShownNetworkView view;
    NetworkSample ethernet = adapter(1, "Ethernet", OperationalStatus::Up);

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{ethernet}));
    ethernet.inBytesPerSec = 4096;
    ethernet.outBytesPerSec = std::nullopt;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{ethernet}));

    const auto* chart = view.adapterCard(0)->chart();
    ASSERT_EQ(chart->seriesCount(), 2u);
    EXPECT_EQ(chart->seriesLabel(0), "Receive");
    EXPECT_EQ(chart->seriesLabel(1), "Send");
    EXPECT_EQ(chart->capacity(), NetworkView::kHistoryCapacity);
    EXPECT_TRUE(chart->isAutoRange());
    const auto receive = chart->seriesSamples(0);
    const auto send = chart->seriesSamples(1);
    EXPECT_EQ(std::vector<std::optional<float>>(receive.begin(), receive.end()),
              (std::vector<std::optional<float>>{2048.0f, 4096.0f}));
    EXPECT_EQ(std::vector<std::optional<float>>(send.begin(), send.end()),
              (std::vector<std::optional<float>>{1024.0f, std::nullopt}));
    EXPECT_EQ(chart->sampleIndex(), 2u);
}

TEST(NetworkView, Hidden_RecordsHistoryShownWhenTheViewIs)
{
    NetworkView view;
    view.setAttribute(Qt::WA_DontShowOnScreen);
    for (int i = 0; i < 3; ++i) {
        view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));
    }

    view.show();

    EXPECT_EQ(view.adapterCard(0)->chart()->seriesSamples(0).size(), 3u);
}

TEST(NetworkView, ShowAllAdapters_NewlyListedAdapterShowsItsHistory)
{
    ShownNetworkView view;
    for (int i = 0; i < 2; ++i) {
        view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
            adapter(1, "Ethernet", OperationalStatus::Up),
            adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, false, false),
        }));
    }

    view.setShowAllAdapters(true);

    ASSERT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1, 2}));
    EXPECT_EQ(view.adapterCard(1)->chart()->seriesSamples(0).size(), 2u);
}

TEST(NetworkView, ReadFails_ChartsRecordGap)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    view.updateSnapshot(snapshotWith(std::nullopt));

    const auto receive = view.adapterCard(0)->chart()->seriesSamples(0);
    ASSERT_EQ(receive.size(), 2u);
    EXPECT_FALSE(receive[1].has_value());
}
