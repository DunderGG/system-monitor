#include <algorithm>
#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

#include "support/sparkline_samples.h"
#include "ui/charts/sparkline_widget.h"

// The pure functions that SparklineWidget draws with: the auto range, line
// segments, grid lines, summary, history window names, series colours and
// the fill's envelope.

using namespace verdandi::ui::charts;
using verdandi::tests::kChartArea;
using verdandi::tests::sineSamples;

namespace
{

using Samples = verdandi::tests::SparklineSamples;

// 60 slots one pixel apart, so grid line positions equal slot numbers.
const QRectF kSlotArea{0.0, 0.0, 59.0, 50.0};

} // namespace

// ---------------------------------------------------------------------------
// autoRange
// ---------------------------------------------------------------------------

TEST(SparklineAutoRange, NoSamples_FallsBackToUnitRange)
{
    const auto range = autoRange(Samples{});

    EXPECT_FLOAT_EQ(range.min, 0.0f);
    EXPECT_FLOAT_EQ(range.max, 1.0f);
}

TEST(SparklineAutoRange, AllZero_FallsBackToUnitRange)
{
    const auto range = autoRange(Samples{0.0f, 0.0f});

    EXPECT_FLOAT_EQ(range.max, 1.0f);
}

TEST(SparklineAutoRange, MixedSamplesWithGaps_UsesLargestWithHeadroom)
{
    const auto range = autoRange(Samples{10.0f, std::nullopt, 100.0f, 40.0f});

    EXPECT_FLOAT_EQ(range.min, 0.0f);
    EXPECT_FLOAT_EQ(range.max, 110.0f);
}

// ---------------------------------------------------------------------------
// sparklineSegments
// ---------------------------------------------------------------------------

TEST(SparklineSegments, NoSamples_NoSegments)
{
    EXPECT_TRUE(sparklineSegments(Samples{}, 5, kChartArea, kPercentRange).empty());
}

TEST(SparklineSegments, FullCapacity_SpansWholeWidthAndMapsValues)
{
    const auto segments = sparklineSegments(Samples{0.0f, 25.0f, 50.0f, 75.0f, 100.0f}, 5, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    const auto& line = segments[0];
    ASSERT_EQ(line.size(), 5);
    EXPECT_DOUBLE_EQ(line[0].x(), 0.0);
    EXPECT_DOUBLE_EQ(line[4].x(), 100.0);
    EXPECT_DOUBLE_EQ(line[0].y(), 50.0); // 0% at the bottom
    EXPECT_DOUBLE_EQ(line[2].y(), 25.0); // 50% in the middle
    EXPECT_DOUBLE_EQ(line[4].y(), 0.0);  // 100% at the top
}

TEST(SparklineSegments, PartialHistory_GrowsInFromTheRight)
{
    const auto segments = sparklineSegments(Samples{10.0f, 20.0f}, 5, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    ASSERT_EQ(segments[0].size(), 2);
    EXPECT_DOUBLE_EQ(segments[0][0].x(), 75.0);
    EXPECT_DOUBLE_EQ(segments[0][1].x(), 100.0);
}

TEST(SparklineSegments, MissingSample_SplitsIntoSegmentsWithGap)
{
    const auto segments =
        sparklineSegments(Samples{10.0f, 20.0f, std::nullopt, 30.0f, 40.0f}, 5, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(segments[0].size(), 2);
    EXPECT_EQ(segments[1].size(), 2);
    EXPECT_DOUBLE_EQ(segments[1][0].x(), 75.0);
}

TEST(SparklineSegments, IsolatedSampleBetweenGaps_SinglePointSegment)
{
    const auto segments = sparklineSegments(Samples{std::nullopt, 50.0f, std::nullopt}, 3, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].size(), 1);
}

TEST(SparklineSegments, MoreSamplesThanCapacity_ShowsOnlyNewest)
{
    const auto segments = sparklineSegments(Samples{0.0f, 0.0f, 100.0f, 100.0f}, 2, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    ASSERT_EQ(segments[0].size(), 2);
    EXPECT_DOUBLE_EQ(segments[0][0].y(), 0.0);
    EXPECT_DOUBLE_EQ(segments[0][1].y(), 0.0);
}

TEST(SparklineSegments, ValuesOutsideRange_Clamped)
{
    const auto segments = sparklineSegments(Samples{-10.0f, 150.0f}, 2, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_DOUBLE_EQ(segments[0][0].y(), 50.0);
    EXPECT_DOUBLE_EQ(segments[0][1].y(), 0.0);
}

TEST(SparklineSegments, MoreSlotsThanPixels_AtMostTwoPointsPerColumnInTimeOrder)
{
    // 1000 slots across 100 pixels: about ten samples per pixel column.
    const auto segments = sparklineSegments(sineSamples(1000), 1000, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    const auto& line = segments[0];
    EXPECT_LE(line.size(), 2 * 101);
    for (qsizetype i = 1; i < line.size(); ++i) {
        EXPECT_LT(line[i - 1].x(), line[i].x());
    }
}

TEST(SparklineSegments, MoreSlotsThanPixels_KeepsSingleSampleSpike)
{
    Samples samples(1000, 10.0f);
    samples[500] = 90.0f;

    const auto segments = sparklineSegments(samples, 1000, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    const auto highest = std::ranges::min_element(segments[0], {}, &QPointF::y);
    EXPECT_DOUBLE_EQ(highest->y(), 5.0); // 90% of a 50-pixel-high area.
}

TEST(SparklineSegments, MoreSlotsThanPixels_GapStillSplitsSegments)
{
    Samples samples(1000, 50.0f);
    samples[500] = std::nullopt;

    const auto segments = sparklineSegments(samples, 1000, kChartArea, kPercentRange);

    EXPECT_EQ(segments.size(), 2u);
}

TEST(SparklineSegments, CapacityOne_PlacesSampleAtRightEdge)
{
    const auto segments = sparklineSegments(Samples{50.0f}, 1, kChartArea, kPercentRange);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_DOUBLE_EQ(segments[0][0].x(), 100.0);
}

// ---------------------------------------------------------------------------
// verticalGridLines
// ---------------------------------------------------------------------------

TEST(SparklineGrid, CapacityBelowTwo_NoLines)
{
    EXPECT_TRUE(verticalGridLines(1, 10, kSlotArea).empty());
}

TEST(SparklineGrid, EmptyArea_NoLines)
{
    EXPECT_TRUE(verticalGridLines(60, 59, QRectF{}).empty());
}

TEST(SparklineGrid, IndexAlignedWithSpacing_TenLinesFromLeftEdge)
{
    // Capacity 60 puts a line on every 6th sample; slot 0 holds sample 0.
    const auto lines = verticalGridLines(60, 59, kSlotArea);

    ASSERT_EQ(lines.size(), 10u);
    EXPECT_DOUBLE_EQ(lines.front(), 0.0);
    EXPECT_DOUBLE_EQ(lines[1], 6.0);
    EXPECT_DOUBLE_EQ(lines.back(), 54.0);
}

TEST(SparklineGrid, NextSample_LinesScrollLeftByOneSlot)
{
    EXPECT_DOUBLE_EQ(verticalGridLines(60, 60, kSlotArea).front(), 5.0);
    EXPECT_DOUBLE_EQ(verticalGridLines(60, 61, kSlotArea).front(), 4.0);
    EXPECT_DOUBLE_EQ(verticalGridLines(60, 60, kSlotArea).back(), 59.0);
}

TEST(SparklineGrid, FullSpacingLater_SameLinesAgain)
{
    EXPECT_EQ(verticalGridLines(60, 59, kSlotArea), verticalGridLines(60, 65, kSlotArea));
}

TEST(SparklineGrid, ShortHistory_OffsetsWithoutUnderflow)
{
    // Sample 0 at the right edge (slot 59): lines on slots 5, 11, ..., 59.
    const auto lines = verticalGridLines(60, 0, kSlotArea);

    ASSERT_FALSE(lines.empty());
    EXPECT_DOUBLE_EQ(lines.front(), 5.0);
    EXPECT_DOUBLE_EQ(lines.back(), 59.0);
}

TEST(SparklineGrid, SmallCapacity_LineOnEverySample)
{
    EXPECT_EQ(verticalGridLines(5, 4, kChartArea).size(), 5u);
}

// ---------------------------------------------------------------------------
// summarizeSeries
// ---------------------------------------------------------------------------

TEST(SparklineSummary, NoSamples_Empty)
{
    const auto summary = summarizeSeries(Samples{}, 60);

    EXPECT_FALSE(summary.current.has_value());
    EXPECT_FALSE(summary.minimum.has_value());
    EXPECT_FALSE(summary.maximum.has_value());
}

TEST(SparklineSummary, Samples_CurrentIsNewestAndExtremesIgnoreGaps)
{
    const auto summary = summarizeSeries(Samples{5.0f, std::nullopt, 1.0f, 9.0f, 3.0f}, 60);

    EXPECT_EQ(summary.current, 3.0f);
    ASSERT_TRUE(summary.minimum.has_value());
    EXPECT_EQ(summary.minimum->index, 2u);
    EXPECT_FLOAT_EQ(summary.minimum->value, 1.0f);
    ASSERT_TRUE(summary.maximum.has_value());
    EXPECT_EQ(summary.maximum->index, 3u);
    EXPECT_FLOAT_EQ(summary.maximum->value, 9.0f);
}

TEST(SparklineSummary, NewestSampleMissing_NoCurrentValue)
{
    const auto summary = summarizeSeries(Samples{5.0f, std::nullopt}, 60);

    EXPECT_FALSE(summary.current.has_value());
    EXPECT_TRUE(summary.maximum.has_value());
}

TEST(SparklineSummary, Ties_ReportNewestOccurrence)
{
    const auto summary = summarizeSeries(Samples{4.0f, 2.0f, 4.0f, 2.0f}, 60);

    ASSERT_TRUE(summary.minimum.has_value());
    ASSERT_TRUE(summary.maximum.has_value());
    EXPECT_EQ(summary.minimum->index, 3u);
    EXPECT_EQ(summary.maximum->index, 2u);
}

TEST(SparklineSummary, MoreSamplesThanCapacity_OnlyVisibleSamplesCount)
{
    const auto summary = summarizeSeries(Samples{100.0f, 1.0f, 2.0f}, 2);

    ASSERT_TRUE(summary.minimum.has_value());
    ASSERT_TRUE(summary.maximum.has_value());
    EXPECT_EQ(summary.maximum->index, 1u);
    EXPECT_FLOAT_EQ(summary.maximum->value, 2.0f);
    EXPECT_EQ(summary.minimum->index, 0u);
}

// ---------------------------------------------------------------------------
// formatHistoryWindow and defaultSeriesColor
// ---------------------------------------------------------------------------

TEST(SparklineHistoryWindow, Windows_UseLargestExactUnit)
{
    using namespace std::chrono_literals;
    EXPECT_EQ(formatHistoryWindow(0s), "0 s");
    EXPECT_EQ(formatHistoryWindow(90s), "90 s");
    EXPECT_EQ(formatHistoryWindow(60s), "1 min");
    EXPECT_EQ(formatHistoryWindow(300s), "5 min");
    EXPECT_EQ(formatHistoryWindow(1800s), "30 min");
    EXPECT_EQ(formatHistoryWindow(7200s), "2 h");
}

TEST(SparklineSeriesColor, FirstFiveSeries_DistinctColors)
{
    for (std::size_t i = 0; i < 5; ++i) {
        for (std::size_t j = i + 1; j < 5; ++j) {
            EXPECT_NE(defaultSeriesColor(i, false), defaultSeriesColor(j, false)) << i << " vs " << j;
            EXPECT_NE(defaultSeriesColor(i, true), defaultSeriesColor(j, true)) << i << " vs " << j;
        }
    }
}

TEST(SparklineSeriesColor, DarkSurface_UsesDarkSteps)
{
    EXPECT_NE(defaultSeriesColor(0, false), defaultSeriesColor(0, true));
}

TEST(SparklineSeriesColor, BeyondPalette_SharedGreyNotRepeatedHue)
{
    EXPECT_EQ(defaultSeriesColor(5, false), defaultSeriesColor(12, false));
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_NE(defaultSeriesColor(5, false), defaultSeriesColor(i, false));
    }
}

// ---------------------------------------------------------------------------
// upperEnvelope
// ---------------------------------------------------------------------------

TEST(SparklineEnvelope, ZigzagWithinColumns_KeepsHighestPointPerColumn)
{
    const QPolygonF line{QPointF{0.2, 40.0}, QPointF{0.6, 10.0}, QPointF{1.1, 30.0}, QPointF{1.9, 45.0}};

    const QPolygonF envelope = upperEnvelope(line, kChartArea);

    ASSERT_EQ(envelope.size(), 2);
    EXPECT_DOUBLE_EQ(envelope[0].x(), 0.2);
    EXPECT_DOUBLE_EQ(envelope[0].y(), 10.0);
    EXPECT_DOUBLE_EQ(envelope[1].x(), 1.1);
    EXPECT_DOUBLE_EQ(envelope[1].y(), 30.0);
}
