#pragma once

#include <optional>

#include "domain/ring_buffer.h"

namespace sysmon::ui
{

/** History of one chart series, oldest first. A missing sample is std::nullopt and shows as a gap. */
using SampleHistory = domain::RingBuffer<std::optional<float>>;

/** Returns true if history holds at least one sample that is not a gap. */
[[nodiscard]] bool hasAnyValue(const SampleHistory& history);

} // namespace sysmon::ui
