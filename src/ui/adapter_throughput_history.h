#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "domain/network_sample.h"
#include "ui/sample_history.h"

namespace sysmon::ui
{

/**
 * Receive and send throughput history of every adapter in the snapshots,
 * keyed by interface ID, for the network charts of the Network tab and the
 * Performance tab.
 *
 * Each record() appends one sample to both series of every adapter it knows,
 * so all histories stay aligned with the snapshot sequence: an adapter's
 * rates, or a gap when a rate cannot be computed (a new adapter has none until
 * its second sample). An adapter missing from a snapshot, or every adapter when
 * the read failed, records a gap, and is dropped once it is missing and its
 * history holds no values. An adapter that comes up therefore shows what it
 * did while it was down.
 *
 * Not thread-safe; use only on the UI thread.
 */
class AdapterThroughputHistory
{
public:
    struct Adapter
    {
        uint64_t interfaceId{0};
        SampleHistory receive;
        SampleHistory send;
    };

    /** Keeps capacity samples per series. Must be > 0. */
    explicit AdapterThroughputHistory(std::size_t capacity);

    /** Appends one snapshot's adapters; std::nullopt means the read failed. */
    void record(const std::optional<std::vector<domain::NetworkSample>>& networks);

    /** Returns the history of the adapter with interfaceId, or nullptr if none is kept. */
    [[nodiscard]] const Adapter* find(uint64_t interfaceId) const;

    /** Number of adapters with a history. */
    [[nodiscard]] std::size_t adapterCount() const;

private:
    std::size_t m_capacity;
    std::vector<Adapter> m_adapters;
};

} // namespace sysmon::ui
