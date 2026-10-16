#include "ui/adapter_throughput_history.h"

#include <algorithm>
#include <cassert>

namespace sysmon::ui
{

namespace
{

std::optional<float> toChartValue(const std::optional<uint64_t>& bytesPerSecond)
{
    return bytesPerSecond ? std::optional<float>{static_cast<float>(*bytesPerSecond)} : std::nullopt;
}

} // namespace

AdapterThroughputHistory::AdapterThroughputHistory(std::size_t capacity) : m_capacity(capacity)
{
    assert(capacity > 0 && "history capacity must be > 0");
}

void AdapterThroughputHistory::record(const std::optional<std::vector<domain::NetworkSample>>& networks)
{
    const auto inSnapshot = [&networks](uint64_t interfaceId) {
        return networks && std::ranges::any_of(*networks, [interfaceId](const domain::NetworkSample& adapter) {
                   return adapter.interfaceId == interfaceId;
               });
    };

    if (networks) {
        for (const auto& adapter : *networks) {
            auto it = std::ranges::find(m_adapters, adapter.interfaceId, &Adapter::interfaceId);
            if (it == m_adapters.end()) {
                it = m_adapters.insert(m_adapters.end(), Adapter{
                                                             .interfaceId = adapter.interfaceId,
                                                             .receive = SampleHistory{m_capacity},
                                                             .send = SampleHistory{m_capacity},
                                                         });
            }
            it->receive.push(toChartValue(adapter.inBytesPerSec));
            it->send.push(toChartValue(adapter.outBytesPerSec));
        }
    }
    for (auto& history : m_adapters) {
        if (!inSnapshot(history.interfaceId)) {
            history.receive.push(std::nullopt);
            history.send.push(std::nullopt);
        }
    }
    // Adapters still in the snapshot are kept even without values: a new
    // adapter has no rate until its second sample.
    std::erase_if(m_adapters, [&inSnapshot](const Adapter& history) {
        return !inSnapshot(history.interfaceId) && !hasAnyValue(history.receive) && !hasAnyValue(history.send);
    });
}

const AdapterThroughputHistory::Adapter* AdapterThroughputHistory::find(uint64_t interfaceId) const
{
    const auto it = std::ranges::find(m_adapters, interfaceId, &Adapter::interfaceId);
    return it != m_adapters.end() ? &*it : nullptr;
}

std::size_t AdapterThroughputHistory::adapterCount() const
{
    return m_adapters.size();
}

} // namespace sysmon::ui
