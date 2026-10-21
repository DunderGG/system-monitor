#include "ui/sample_history.h"

#include <algorithm>

namespace verdandi::ui
{

bool hasAnyValue(const SampleHistory& history)
{
    return std::ranges::any_of(history, [](const std::optional<float>& sample) { return sample.has_value(); });
}

} // namespace verdandi::ui
