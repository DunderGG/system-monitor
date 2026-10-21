#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "domain/network_sample.h"
#include "ui/network_formatting.h"

// Checks of the adapter lists that the Network tab and the Performance tab's
// network page show (ui::listedAdapters() and ui::chartedAdapters()).

namespace verdandi::tests
{

/** Returns the interface IDs of adapters in ascending order, to check which adapters a list keeps. */
[[nodiscard]] inline std::vector<uint64_t> sortedInterfaceIds(const std::vector<const domain::NetworkSample*>& adapters)
{
    std::vector<uint64_t> ids;
    for (const auto* adapter : adapters) {
        ids.push_back(adapter->interfaceId);
    }
    std::ranges::sort(ids);
    return ids;
}

/** Returns true if adapters are in ui::isListedBefore() order. */
[[nodiscard]] inline bool isInListedOrder(const std::vector<const domain::NetworkSample*>& adapters)
{
    return std::ranges::is_sorted(
        adapters, ui::isListedBefore,
        [](const domain::NetworkSample* adapter) -> const domain::NetworkSample& { return *adapter; });
}

} // namespace verdandi::tests
