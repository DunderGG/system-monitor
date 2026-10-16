#include "ui/network_formatting.h"

#include <algorithm>

#include <QString>

#include "ui/performance_formatting.h"

namespace sysmon::ui
{

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

} // namespace sysmon::ui
