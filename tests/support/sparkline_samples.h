#pragma once

#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <QRectF>

#include "ui/charts/sparkline_widget.h"

// Sample histories and plot areas that the sparkline geometry and widget tests share.

namespace verdandi::tests
{

/** One series of a sparkline, oldest first; std::nullopt is a gap. */
using SparklineSamples = std::vector<std::optional<float>>;

/** The plot area that the geometry tests map samples into. */
inline const QRectF kChartArea{0.0, 0.0, 100.0, 50.0};

/** Returns count samples of a sine wave between 10 and 90, so no sample reaches the ends of ui::charts::kPercentRange.
 */
[[nodiscard]] inline SparklineSamples sineSamples(std::size_t count)
{
    SparklineSamples samples;
    samples.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        samples.push_back(50.0f + 40.0f * static_cast<float>(std::sin(static_cast<double>(i) * 0.1)));
    }
    return samples;
}

} // namespace verdandi::tests
