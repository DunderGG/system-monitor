#include "ui/network_formatting.h"

#include <algorithm>

#include <QStringList>

#include "ui/dashboard_formatting.h"
#include "ui/performance_formatting.h"

namespace sysmon::ui
{

namespace
{

bool isIpv6(const std::string& address)
{
    return address.find(':') != std::string::npos;
}

QString addressesOfFamily(const domain::NetworkSample& adapter, bool ipv6)
{
    std::vector<std::string> addresses;
    for (const auto& address : adapter.ipAddresses) {
        if (isIpv6(address) == ipv6) {
            addresses.push_back(address);
        }
    }
    return formatAddressList(addresses);
}

} // namespace

bool isListedByDefault(const domain::NetworkSample& adapter)
{
    return adapter.isHardwareInterface || adapter.hasIpInterface;
}

std::vector<const domain::NetworkSample*> listedAdapters(const std::vector<domain::NetworkSample>& networks,
                                                         bool showAll)
{
    std::vector<const domain::NetworkSample*> listed;
    for (const auto& adapter : networks) {
        if (showAll || isListedByDefault(adapter)) {
            listed.push_back(&adapter);
        }
    }
    std::ranges::sort(listed, [](const domain::NetworkSample* left, const domain::NetworkSample* right) {
        const bool leftUp = left->operationalStatus == domain::OperationalStatus::Up;
        const bool rightUp = right->operationalStatus == domain::OperationalStatus::Up;
        if (leftUp != rightUp) {
            return leftUp;
        }
        if (left->isHardwareInterface != right->isHardwareInterface) {
            return left->isHardwareInterface;
        }
        const int byName = QString::compare(adapterDisplayName(*left), adapterDisplayName(*right), Qt::CaseInsensitive);
        if (byName != 0) {
            return byName < 0;
        }
        return left->interfaceId < right->interfaceId;
    });
    return listed;
}

QString operationalStatusText(domain::OperationalStatus status)
{
    switch (status) {
        case domain::OperationalStatus::Up:
            return "Up";
        case domain::OperationalStatus::Down:
            return "Down";
        case domain::OperationalStatus::Testing:
            return "Testing";
        case domain::OperationalStatus::Dormant:
            return "Dormant";
        case domain::OperationalStatus::NotPresent:
            return "Not present";
        case domain::OperationalStatus::LowerLayerDown:
            return "Lower layer down";
        case domain::OperationalStatus::Unknown:
        default:
            return "Unknown";
    }
}

QString adapterTypeText(const domain::NetworkSample& adapter)
{
    QString text;
    switch (adapter.type) {
        case domain::AdapterType::Ethernet:
            text = "Ethernet";
            break;
        case domain::AdapterType::WiFi:
            text = "Wi-Fi";
            break;
        case domain::AdapterType::Bluetooth:
            text = "Bluetooth";
            break;
        case domain::AdapterType::MobileBroadband:
            text = "Mobile broadband";
            break;
        // Tunnels and PPP links are always virtual, so they need no mark.
        case domain::AdapterType::Tunnel:
            return "Tunnel";
        case domain::AdapterType::Ppp:
            return "PPP";
        case domain::AdapterType::Other:
        default:
            return adapter.isHardwareInterface ? "Other" : "Virtual";
    }
    return adapter.isHardwareInterface ? text : QString("%1 (virtual)").arg(text);
}

QString formatAddressList(const std::vector<std::string>& addresses)
{
    if (addresses.empty()) {
        return "None";
    }
    QStringList lines;
    for (const auto& address : addresses) {
        lines.append(QString::fromStdString(address));
    }
    return lines.join('\n');
}

QString ipv4AddressText(const domain::NetworkSample& adapter)
{
    return addressesOfFamily(adapter, false);
}

QString ipv6AddressText(const domain::NetworkSample& adapter)
{
    return addressesOfFamily(adapter, true);
}

QString networkCategoryText(domain::NetworkCategory category)
{
    switch (category) {
        case domain::NetworkCategory::Public:
            return "Public";
        case domain::NetworkCategory::Private:
            return "Private";
        case domain::NetworkCategory::DomainAuthenticated:
        default:
            return "Domain";
    }
}

QString networkProfileText(const domain::NetworkProfile& profile)
{
    const QString name = QString::fromStdString(profile.name);
    return profile.category ? QString("%1 (%2)").arg(name, networkCategoryText(*profile.category)) : name;
}

QString adapterNetworkText(uint64_t interfaceId, const std::optional<std::vector<domain::NetworkProfile>>& profiles)
{
    if (!profiles) {
        return kNotAvailable;
    }
    QStringList lines;
    for (const auto& profile : *profiles) {
        if (std::ranges::find(profile.interfaceIds, interfaceId) != profile.interfaceIds.end()) {
            lines.append(networkProfileText(profile));
        }
    }
    return lines.isEmpty() ? QString("None") : lines.join('\n');
}

std::vector<Readout> adapterDetailReadouts(const std::optional<domain::NetworkSample>& adapter,
                                           const std::optional<std::vector<domain::NetworkProfile>>& profiles)
{
    if (!adapter) {
        return {
            {"Network", kNotAvailable},      {"Type", kNotAvailable},         {"Link speed", kNotAvailable},
            {"IPv4 address", kNotAvailable}, {"IPv6 address", kNotAvailable}, {"DNS servers", kNotAvailable},
        };
    }

    QString linkSpeed = "Not connected";
    if (adapter->operationalStatus == domain::OperationalStatus::Up) {
        // A link speed of 0 means the adapter did not report one.
        linkSpeed = adapter->linkSpeedBps > 0 ? formatLinkSpeed(adapter->linkSpeedBps) : kNotAvailable;
    }
    return {
        {"Network", adapterNetworkText(adapter->interfaceId, profiles)},
        {"Type", adapterTypeText(*adapter)},
        {"Link speed", linkSpeed},
        {"IPv4 address", ipv4AddressText(*adapter)},
        {"IPv6 address", ipv6AddressText(*adapter)},
        {"DNS servers", formatAddressList(adapter->dnsServers)},
    };
}

std::vector<Readout> connectivityReadouts(const domain::ConnectivityStatus& status,
                                          const std::optional<std::vector<domain::NetworkProfile>>& profiles)
{
    QString networks = kNotAvailable;
    if (profiles) {
        QStringList lines;
        for (const auto& profile : *profiles) {
            lines.append(networkProfileText(profile));
        }
        networks = lines.isEmpty() ? QString("None") : lines.join('\n');
    }
    QString cost = "Unknown";
    if (status.isMetered) {
        cost = *status.isMetered ? "Metered" : "Unmetered";
    }
    QString roaming = "Unknown";
    if (status.isRoaming) {
        roaming = *status.isRoaming ? "Yes" : "No";
    }
    return {
        {"Connectivity", connectivityLevelText(status.level)},
        {"Cost", cost},
        {"Roaming", roaming},
        {"Networks", networks},
    };
}

} // namespace sysmon::ui
