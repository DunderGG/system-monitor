#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QLabel>
#include <QSize>
#include <QTabWidget>

#include "domain/system_snapshot.h"
#include "text_contrast.h"
#include "ui/adapter_card.h"
#include "ui/charts/sparkline_widget.h"
#include "ui/main_window.h"
#include "ui/network_view.h"
#include "ui/readout_grid.h"
#include "ui/text_tone.h"

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

// Text from outside the program that looks like HTML: an adapter's alias and
// description, which drivers and VPN clients choose, and the names of two
// networks, short enough to be SSIDs that an access point chooses.
SystemSnapshot snapshotWithMarkup()
{
    NetworkSample ethernet = adapter(1, "<h1>Ethernet</h1>", OperationalStatus::Up);
    ethernet.description = "<i>Realtek</i> <img height=3000>";
    SystemSnapshot snapshot = snapshotWith(std::vector<NetworkSample>{ethernet});
    snapshot.networkProfiles = std::vector<NetworkProfile>{
        {.name = "<h1>FREE</h1>", .category = NetworkCategory::Public, .interfaceIds = {1}},
        {.name = "<img height=3000>", .category = NetworkCategory::Public, .interfaceIds = {1}},
    };
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
    EXPECT_EQ(view.hiddenAdaptersHeadingPosition(), std::optional<std::size_t>{1});

    view.setShowAllAdapters(false);

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1}));
    EXPECT_FALSE(view.hiddenAdaptersHeadingPosition().has_value());
}

TEST(NetworkView, ShowAll_AdaptersWindowsHidesListedUnderAHeadingAfterTheOthers)
{
    ShownNetworkView view;
    NetworkSample wifiDirect = adapter(3, "Local Area Connection* 1", OperationalStatus::Down, false, true);
    wifiDirect.isHidden = true;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "WAN Miniport (IP)", OperationalStatus::Up, false, false),
        wifiDirect,
        adapter(4, "Bluetooth Network Connection", OperationalStatus::Down, false, true),
    }));

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1, 4}));
    EXPECT_FALSE(view.hiddenAdaptersHeadingPosition().has_value());

    view.setShowAllAdapters(true);

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1, 4, 3, 2}));
    EXPECT_EQ(view.hiddenAdaptersHeadingPosition(), std::optional<std::size_t>{2});
}

TEST(NetworkView, AdapterChangesStatus_KeepsItsPlaceAndCard)
{
    ShownNetworkView view;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "Wi-Fi", OperationalStatus::Down),
    }));
    AdapterCard* ethernetCard = view.adapterCard(0);
    AdapterCard* wifiCard = view.adapterCard(1);

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Down),
        adapter(2, "Wi-Fi", OperationalStatus::Up),
    }));

    EXPECT_EQ(view.listedInterfaceIds(), (std::vector<uint64_t>{1, 2}));
    EXPECT_EQ(view.adapterCard(0), ethernetCard);
    EXPECT_EQ(view.adapterCard(1), wifiCard);
}

TEST(NetworkView, Card_NotUp_ChartCollapsedUntilTheAdapterIsUp)
{
    ShownNetworkView view;
    NetworkSample wifi = adapter(1, "Wi-Fi", OperationalStatus::Down);
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{wifi}));
    const AdapterCard* card = view.adapterCard(0);

    EXPECT_TRUE(card->chart()->isHidden());

    wifi.operationalStatus = OperationalStatus::Up;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{wifi}));

    EXPECT_FALSE(card->chart()->isHidden());
    // Recorded while collapsed, so the chart shows the minute it was down too.
    EXPECT_EQ(card->chart()->seriesSamples(0).size(), 2u);
    EXPECT_EQ(card->chart()->sampleIndex(), 2u);
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

TEST(NetworkView, Card_DisconnectedThenUp_AddressesShownOnlyWhileUp)
{
    ShownNetworkView view;
    NetworkSample wifi = adapter(1, "WiFi", OperationalStatus::Down);
    wifi.ipAddresses = {"192.168.1.16"};
    wifi.dnsServers = {"192.168.1.1"};

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{wifi}));

    const AdapterCard* card = view.adapterCard(0);
    EXPECT_EQ(card->readoutGrid()->value("IPv4 address"), "Not connected");
    EXPECT_EQ(card->readoutGrid()->value("IPv6 address"), "Not connected");
    EXPECT_EQ(card->readoutGrid()->value("DNS servers"), "Not connected");

    wifi.operationalStatus = OperationalStatus::Up;
    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{wifi}));

    EXPECT_EQ(card->readoutGrid()->value("IPv4 address"), "192.168.1.16");
    EXPECT_EQ(card->readoutGrid()->value("IPv6 address"), "None");
    EXPECT_EQ(card->readoutGrid()->value("DNS servers"), "192.168.1.1");
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

TEST(NetworkView, Connectivity_PlaceholderBeforeFirstSnapshotThenReadouts)
{
    ShownNetworkView view;

    EXPECT_EQ(view.connectivityReadoutGrid()->value("Connectivity"), "--");

    SystemSnapshot snapshot = snapshotWith(std::vector<NetworkSample>{});
    snapshot.connectivity = {.level = ConnectivityLevel::InternetAccess, .isMetered = true, .isRoaming = false};
    view.updateSnapshot(snapshot);

    EXPECT_EQ(view.connectivityReadoutGrid()->value("Connectivity"), "Internet access");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Cost"), "Metered");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Roaming"), "No");
}

TEST(NetworkView, Connectivity_UnknownBeforeTheFirstSlowRefresh_ShownAsUnknown)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{}));

    EXPECT_EQ(view.connectivityReadoutGrid()->value("Connectivity"), "Unknown");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Cost"), "Unknown");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Roaming"), "Unknown");
}

TEST(NetworkView, Connectivity_ChangesWhileHidden_ShownWhenTheViewIs)
{
    NetworkView view;
    view.setAttribute(Qt::WA_DontShowOnScreen);
    SystemSnapshot snapshot = snapshotWith(std::vector<NetworkSample>{});
    snapshot.connectivity = {.level = ConnectivityLevel::LocalAccess, .isMetered = false, .isRoaming = false};
    view.updateSnapshot(snapshot);

    view.show();

    EXPECT_EQ(view.connectivityReadoutGrid()->value("Connectivity"), "Local network only");
}

TEST(NetworkView, NetworkProfiles_ShownOnTheirAdaptersAndInConnectivity)
{
    ShownNetworkView view;
    SystemSnapshot snapshot = snapshotWith(std::vector<NetworkSample>{
        adapter(1, "Ethernet", OperationalStatus::Up),
        adapter(2, "Wi-Fi", OperationalStatus::Down),
    });
    snapshot.networkProfiles = std::vector<NetworkProfile>{
        {.name = "Normandy", .category = NetworkCategory::Private, .interfaceIds = {1}},
    };

    view.updateSnapshot(snapshot);

    EXPECT_EQ(view.adapterCard(0)->readoutGrid()->value("Network"), "Normandy (Private)");
    EXPECT_EQ(view.adapterCard(1)->readoutGrid()->value("Network"), "None");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Networks"), "Normandy (Private)");
}

TEST(NetworkView, NetworkProfilesUnreadable_NotAvailable)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWith(std::vector<NetworkSample>{adapter(1, "Ethernet", OperationalStatus::Up)}));

    EXPECT_EQ(view.adapterCard(0)->readoutGrid()->value("Network"), "N/A");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Networks"), "N/A");
}

TEST(NetworkView, MarkupInNamesAndDescription_ShownAsPlainText)
{
    ShownNetworkView view;

    view.updateSnapshot(snapshotWithMarkup());

    const AdapterCard* card = view.adapterCard(0);
    EXPECT_EQ(card->title(), "<h1>Ethernet</h1>");
    EXPECT_EQ(card->description(), "<i>Realtek</i> <img height=3000>");
    EXPECT_EQ(card->readoutGrid()->value("Network"), "<h1>FREE</h1> (Public)\n<img height=3000> (Public)");
    EXPECT_EQ(view.connectivityReadoutGrid()->value("Networks"), "<h1>FREE</h1> (Public)\n<img height=3000> (Public)");
    for (const auto* label : card->findChildren<QLabel*>()) {
        EXPECT_EQ(label->textFormat(), Qt::PlainText) << label->text().toStdString();
    }
    for (const auto* label : view.connectivityReadoutGrid()->findChildren<QLabel*>()) {
        EXPECT_EQ(label->textFormat(), Qt::PlainText) << label->text().toStdString();
    }
}

TEST(NetworkView, MarkupInNamesAndDescription_WindowMinimumSizeStaysWithin800x600)
{
    MainWindow mainWindow;
    mainWindow.setAttribute(Qt::WA_DontShowOnScreen);
    mainWindow.resize(800, 600);
    mainWindow.show();
    mainWindow.tabWidget()->setCurrentWidget(mainWindow.networkView());

    mainWindow.onSnapshotReady(snapshotWithMarkup());
    QCoreApplication::processEvents();

    ASSERT_EQ(mainWindow.networkView()->listedInterfaceIds(), (std::vector<uint64_t>{1}));
    const QSize minimum = mainWindow.minimumSizeHint();
    EXPECT_LE(minimum.width(), 800);
    EXPECT_LE(minimum.height(), 600);
    EXPECT_EQ(mainWindow.size(), QSize(800, 600));
}

TEST(NetworkView, Card_StatusDescriptionAndReadoutNamesReadableInBothColourSchemes)
{
    // One window for both schemes, so the colours must follow a change of scheme.
    MainWindow mainWindow;
    mainWindow.setAttribute(Qt::WA_DontShowOnScreen);
    // Tall enough that both cards are in view, so every label is rendered.
    mainWindow.resize(1024, 1200);
    mainWindow.show();
    mainWindow.tabWidget()->setCurrentWidget(mainWindow.networkView());
    NetworkSample ethernet = adapter(1, "Ethernet", OperationalStatus::Up);
    ethernet.description = "Realtek Gaming 2.5GbE Family Controller";
    NetworkSample wifi = adapter(2, "WiFi", OperationalStatus::Down);
    wifi.description = "RZ616 Wi-Fi 6E 160MHz";
    mainWindow.onSnapshotReady(snapshotWith(std::vector<NetworkSample>{ethernet, wifi}));

    for (const auto scheme : {Qt::ColorScheme::Light, Qt::ColorScheme::Dark}) {
        const sysmon::test::ScopedColorScheme colorScheme(scheme);
        QCoreApplication::processEvents();

        const NetworkView* view = mainWindow.networkView();
        ASSERT_EQ(view->listedInterfaceIds(), (std::vector<uint64_t>{1, 2}));
        for (std::size_t index = 0; index < 2; ++index) {
            const AdapterCard* card = view->adapterCard(index);
            const auto readoutNames = card->readoutGrid()->findChildren<TonedLabel*>();
            ASSERT_FALSE(readoutNames.isEmpty());
            for (const QWidget* label : {static_cast<const QWidget*>(card->statusLabel()),
                                         static_cast<const QWidget*>(card->descriptionLabel()),
                                         static_cast<const QWidget*>(readoutNames.front())}) {
                EXPECT_GE(sysmon::test::renderedTextContrast(mainWindow, *label), sysmon::test::kMinimumTextContrast)
                    << (scheme == Qt::ColorScheme::Dark ? "dark: " : "light: ")
                    << qobject_cast<const QLabel*>(label)->text().toStdString();
            }
        }
        EXPECT_EQ(view->adapterCard(0)->statusLabel()->tone(), TextTone::Good);
        EXPECT_EQ(view->adapterCard(1)->statusLabel()->tone(), TextTone::Dimmed);
    }
}
