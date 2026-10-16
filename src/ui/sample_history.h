#pragma once

#include <cstddef>
#include <optional>

#include "domain/ring_buffer.h"

namespace sysmon::ui
{

/** History of one chart series, oldest first. A missing sample is std::nullopt and shows as a gap. */
using SampleHistory = domain::RingBuffer<std::optional<float>>;

/** Samples in one minute of history at the default 1 Hz sampling rate, as the dashboard and the Network tab chart. */
inline constexpr std::size_t kOneMinuteOfSamples = 60;

/** Returns true if history holds at least one sample that is not a gap. */
[[nodiscard]] bool hasAnyValue(const SampleHistory& history);

} // namespace sysmon::ui
