#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QString>

#include "ui/charts/sparkline_widget.h"

using namespace sysmon::ui::charts;

namespace
{

using Samples = std::vector<std::optional<float>>;

const QRectF kArea{0.0, 0.0, 100.0, 50.0};
// 60 slots one pixel apart, so grid line positions equal slot numbers.
const QRectF kSlotArea{0.0, 0.0, 59.0, 50.0};
constexpr YRange kPercent{.min = 0.0f, .max = 100.0f};

// True if any pixel in region of the image is not fully transparent.
bool hasPaintedPixel(const QImage& image, const QRect& region)
{
    const QRect bounded = region.intersected(image.rect());
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                return true;
            }
        }
    }
    return false;
}

bool hasPaintedPixel(const QImage& image)
{
    return hasPaintedPixel(image, image.rect());
}

// Renders only the widget's own painting onto a transparent image.
QImage renderWidget(SparklineWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image, QPoint(), QRegion(), QWidget::DrawChildren); // Skip the window background.
    return image;
}

Samples sineSamples(std::size_t count)
{
    Samples samples;
    samples.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        samples.push_back(50.0f + 40.0f * static_cast<float>(std::sin(static_cast<double>(i) * 0.1)));
    }
    return samples;
}

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

TEST(SparklineAutoRange, UsesLargestSampleWithHeadroomAndIgnoresGaps)
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
    EXPECT_TRUE(sparklineSegments(Samples{}, 5, kArea, kPercent).empty());
}

TEST(SparklineSegments, FullCapacity_SpansWholeWidthAndMapsValues)
{
    const auto segments = sparklineSegments(Samples{0.0f, 25.0f, 50.0f, 75.0f, 100.0f}, 5, kArea, kPercent);

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
    const auto segments = sparklineSegments(Samples{10.0f, 20.0f}, 5, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    ASSERT_EQ(segments[0].size(), 2);
    EXPECT_DOUBLE_EQ(segments[0][0].x(), 75.0);
    EXPECT_DOUBLE_EQ(segments[0][1].x(), 100.0);
}

TEST(SparklineSegments, MissingSample_SplitsIntoSegmentsWithGap)
{
    const auto segments = sparklineSegments(Samples{10.0f, 20.0f, std::nullopt, 30.0f, 40.0f}, 5, kArea, kPercent);

    ASSERT_EQ(segments.size(), 2u);
    EXPECT_EQ(segments[0].size(), 2);
    EXPECT_EQ(segments[1].size(), 2);
    EXPECT_DOUBLE_EQ(segments[1][0].x(), 75.0);
}

TEST(SparklineSegments, IsolatedSampleBetweenGaps_SinglePointSegment)
{
    const auto segments = sparklineSegments(Samples{std::nullopt, 50.0f, std::nullopt}, 3, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(segments[0].size(), 1);
}

TEST(SparklineSegments, MoreSamplesThanCapacity_ShowsOnlyNewest)
{
    const auto segments = sparklineSegments(Samples{0.0f, 0.0f, 100.0f, 100.0f}, 2, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    ASSERT_EQ(segments[0].size(), 2);
    EXPECT_DOUBLE_EQ(segments[0][0].y(), 0.0);
    EXPECT_DOUBLE_EQ(segments[0][1].y(), 0.0);
}

TEST(SparklineSegments, ValuesOutsideRange_Clamped)
{
    const auto segments = sparklineSegments(Samples{-10.0f, 150.0f}, 2, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_DOUBLE_EQ(segments[0][0].y(), 50.0);
    EXPECT_DOUBLE_EQ(segments[0][1].y(), 0.0);
}

TEST(SparklineSegments, MoreSlotsThanPixels_AtMostTwoPointsPerColumnInTimeOrder)
{
    // 1000 slots across 100 pixels: about ten samples per pixel column.
    const auto segments = sparklineSegments(sineSamples(1000), 1000, kArea, kPercent);

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

    const auto segments = sparklineSegments(samples, 1000, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    const auto highest = std::ranges::min_element(segments[0], {}, &QPointF::y);
    EXPECT_DOUBLE_EQ(highest->y(), 5.0); // 90% of a 50-pixel-high area.
}

TEST(SparklineSegments, MoreSlotsThanPixels_GapStillSplitsSegments)
{
    Samples samples(1000, 50.0f);
    samples[500] = std::nullopt;

    const auto segments = sparklineSegments(samples, 1000, kArea, kPercent);

    EXPECT_EQ(segments.size(), 2u);
}

TEST(SparklineSegments, CapacityOne_PlacesSampleAtRightEdge)
{
    const auto segments = sparklineSegments(Samples{50.0f}, 1, kArea, kPercent);

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
    EXPECT_EQ(verticalGridLines(5, 4, kArea).size(), 5u);
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
// Buffer sizes from the roadmap (60, 300, 1800 samples)
// ---------------------------------------------------------------------------

class SparklineBufferSize : public ::testing::TestWithParam<std::size_t>
{};

TEST_P(SparklineBufferSize, FullSyntheticHistory_ProducesOneSegmentAcrossWidth)
{
    const std::size_t size = GetParam();
    const Samples samples = sineSamples(size);

    const auto segments = sparklineSegments(samples, size, kArea, kPercent);

    // Up to 100 samples, one point each; beyond the 100-pixel width, at most
    // two per pixel column (101 columns including the right edge).
    ASSERT_EQ(segments.size(), 1u);
    const auto pointCount = static_cast<std::size_t>(segments[0].size());
    if (size <= 100) {
        EXPECT_EQ(pointCount, size);
    }
    EXPECT_LE(pointCount, 2u * 101u);
    EXPECT_GE(segments[0].front().x(), kArea.left());
    EXPECT_LT(segments[0].front().x(), kArea.left() + 1.0);
    EXPECT_DOUBLE_EQ(segments[0].back().x(), kArea.right());
}

TEST_P(SparklineBufferSize, FullSyntheticHistory_PaintsWithoutError)
{
    const std::size_t size = GetParam();
    Samples samples(size, 42.0f);
    SparklineWidget widget;
    widget.resize(160, 36);
    widget.setCapacity(size);
    widget.setSamples(samples);

    EXPECT_TRUE(hasPaintedPixel(renderWidget(widget)));
}

TEST_P(SparklineBufferSize, FullHistory_TenGridColumns)
{
    const std::size_t size = GetParam();

    EXPECT_EQ(verticalGridLines(size, size - 1, kArea).size(), 10u);
}

TEST_P(SparklineBufferSize, FullDecoratedChartWithTwoSeries_PaintsWithoutError)
{
    const std::size_t size = GetParam();
    SparklineWidget widget;
    widget.resize(480, 200);
    widget.setCapacity(size);
    widget.setSeriesCount(2);
    widget.setSeriesSamples(0, sineSamples(size));
    widget.setSeriesSamples(1, Samples(size, 20.0f));
    widget.setSampleIndex(size * 3);
    widget.setGridVisible(true);
    widget.setAxisLabelsVisible(true);
    widget.setAnnotationsVisible(true);

    EXPECT_TRUE(hasPaintedPixel(renderWidget(widget)));
}

INSTANTIATE_TEST_SUITE_P(RoadmapSizes, SparklineBufferSize, ::testing::Values(60u, 300u, 1800u));

// ---------------------------------------------------------------------------
// SparklineWidget
// ---------------------------------------------------------------------------

TEST(SparklineWidget, Defaults_FixedPercentRangeAndSixtySlots)
{
    SparklineWidget widget;

    EXPECT_FALSE(widget.isAutoRange());
    EXPECT_FLOAT_EQ(widget.effectiveRange().max, 100.0f);
    EXPECT_EQ(widget.capacity(), 60u);
    EXPECT_TRUE(widget.samples().empty());
}

TEST(SparklineWidget, Defaults_OneFilledSeriesAndNoDecorations)
{
    SparklineWidget widget;

    EXPECT_EQ(widget.seriesCount(), 1u);
    EXPECT_TRUE(widget.isSeriesFilled(0));
    EXPECT_TRUE(widget.seriesLabel(0).isEmpty());
    EXPECT_FALSE(widget.isGridVisible());
    EXPECT_FALSE(widget.areAxisLabelsVisible());
    EXPECT_FALSE(widget.areAnnotationsVisible());
    EXPECT_EQ(widget.sampleIndex(), 0u);
    EXPECT_EQ(widget.sampleInterval(), std::chrono::seconds{1});
}

TEST(SparklineWidget, AutoRange_FollowsSamples)
{
    SparklineWidget widget;
    widget.setAutoRange();

    widget.setSamples(Samples{200.0f, 400.0f});

    EXPECT_TRUE(widget.isAutoRange());
    EXPECT_FLOAT_EQ(widget.effectiveRange().max, 440.0f);
}

TEST(SparklineWidget, AutoRange_CoversAllSeries)
{
    SparklineWidget widget;
    widget.setAutoRange();
    widget.setSeriesCount(2);

    widget.setSeriesSamples(0, Samples{10.0f});
    widget.setSeriesSamples(1, Samples{std::nullopt, 200.0f});

    EXPECT_FLOAT_EQ(widget.effectiveRange().max, 220.0f);
}

TEST(SparklineWidget, SetSamples_KeepsCopyIncludingGaps)
{
    SparklineWidget widget;
    Samples samples{1.0f, std::nullopt, 3.0f};

    widget.setSamples(samples);
    samples.clear();

    ASSERT_EQ(widget.samples().size(), 3u);
    EXPECT_FALSE(widget.samples()[1].has_value());
}

TEST(SparklineWidget, SetSamples_UpdatesPrimarySeries)
{
    SparklineWidget widget;
    widget.setSeriesCount(2);

    widget.setSamples(Samples{7.0f});

    ASSERT_EQ(widget.seriesSamples(0).size(), 1u);
    EXPECT_EQ(widget.seriesSamples(0)[0], 7.0f);
    EXPECT_TRUE(widget.seriesSamples(1).empty());
}

TEST(SparklineWidget, SetSeriesCount_KeepsExistingSeriesAndAddsUnfilledOnes)
{
    SparklineWidget widget;
    widget.setSamples(Samples{1.0f});
    widget.setSeriesLabel(0, "Total");

    widget.setSeriesCount(3);

    EXPECT_EQ(widget.seriesCount(), 3u);
    EXPECT_EQ(widget.samples().size(), 1u);
    EXPECT_EQ(widget.seriesLabel(0), "Total");
    EXPECT_TRUE(widget.isSeriesFilled(0));
    EXPECT_FALSE(widget.isSeriesFilled(1));
    EXPECT_TRUE(widget.seriesSamples(2).empty());
}

TEST(SparklineWidget, SeriesColor_DefaultUntilOverridden)
{
    SparklineWidget widget;
    widget.setSeriesCount(2);
    EXPECT_NE(widget.seriesColor(0), widget.seriesColor(1));

    widget.setSeriesColor(1, QColor(Qt::black));

    EXPECT_EQ(widget.seriesColor(1), QColor(Qt::black));
}

TEST(SparklineWidget, ValueFormatter_UsedForLabels)
{
    SparklineWidget widget;
    EXPECT_EQ(widget.formatValue(100.0f), "100");
    EXPECT_EQ(widget.formatValue(0.0f), "0");
    EXPECT_EQ(widget.formatValue(2.5f), "2.5");

    widget.setValueFormatter([](float value) { return QString("%1%").arg(static_cast<double>(value)); });

    EXPECT_EQ(widget.formatValue(42.0f), "42%");
}

TEST(SparklineWidget, PlotArea_LabelsAndAnnotationsReserveSpace)
{
    SparklineWidget widget;
    widget.resize(300, 150);
    const QRectF plain = widget.plotArea();

    widget.setAxisLabelsVisible(true);
    const QRectF labelled = widget.plotArea();
    widget.setAnnotationsVisible(true);
    const QRectF annotated = widget.plotArea();

    EXPECT_GT(labelled.left(), plain.left());
    EXPECT_LT(labelled.bottom(), plain.bottom());
    EXPECT_DOUBLE_EQ(labelled.top(), plain.top());
    EXPECT_GT(annotated.top(), labelled.top());
}

TEST(SparklineWidget, NoSamples_PaintsNothing)
{
    SparklineWidget widget;
    widget.resize(160, 36);

    EXPECT_FALSE(hasPaintedPixel(renderWidget(widget)));
}

TEST(SparklineWidget, GridVisible_PaintsWithoutSamples)
{
    SparklineWidget widget;
    widget.resize(160, 60);
    widget.setGridVisible(true);

    EXPECT_TRUE(hasPaintedPixel(renderWidget(widget)));
}

TEST(SparklineWidget, SampleIndex_ScrollsGrid)
{
    SparklineWidget widget;
    widget.resize(300, 100);
    widget.setGridVisible(true);
    widget.setSampleIndex(59);
    const QImage before = renderWidget(widget);

    widget.setSampleIndex(60);
    const QImage after = renderWidget(widget);
    widget.setSampleIndex(65);
    const QImage fullSpacingLater = renderWidget(widget);

    EXPECT_NE(before, after);
    EXPECT_EQ(before, fullSpacingLater);
}

TEST(SparklineWidget, AxisLabelsVisible_PaintsInGutterAndBelowPlot)
{
    SparklineWidget widget;
    widget.resize(300, 120);
    widget.setAxisLabelsVisible(true);
    const QRect plot = widget.plotArea().toAlignedRect();

    const QImage image = renderWidget(widget);

    EXPECT_TRUE(hasPaintedPixel(image, QRect(0, 0, plot.left() - 2, widget.height())));
    EXPECT_TRUE(hasPaintedPixel(image, QRect(0, plot.bottom() + 2, widget.width(), widget.height())));
}

TEST(SparklineWidget, AnnotationsVisible_PaintsAbovePlot)
{
    SparklineWidget widget;
    widget.resize(300, 120);
    widget.setAnnotationsVisible(true);
    widget.setSamples(Samples{10.0f, 90.0f, 50.0f});
    const QRect plot = widget.plotArea().toAlignedRect();

    const QImage image = renderWidget(widget);

    // The band above the plot, excluding where the max marker may reach into it.
    EXPECT_TRUE(hasPaintedPixel(image, QRect(0, 0, widget.width(), plot.top() - 8)));
}

TEST(SparklineWidget, AnnotationsWithoutSamples_ShowNotAvailable)
{
    SparklineWidget widget;
    widget.resize(300, 120);
    widget.setAnnotationsVisible(true);
    const QRect plot = widget.plotArea().toAlignedRect();

    const QImage image = renderWidget(widget);

    EXPECT_TRUE(hasPaintedPixel(image, QRect(0, 0, widget.width(), plot.top() - 8)));
}

TEST(SparklineWidget, MoreSlotsThanPixels_StillFillsBelowTheLine)
{
    SparklineWidget widget;
    widget.resize(200, 100);
    widget.setCapacity(1000);
    widget.setSamples(sineSamples(1000));
    const QRect plot = widget.plotArea().toAlignedRect();
    const QRect bottomBand(plot.left(), plot.bottom() - 5, plot.width(), 5);

    EXPECT_TRUE(hasPaintedPixel(renderWidget(widget), bottomBand)); // Gradient fill reaches the bottom.
}

TEST(SparklineWidget, UnfilledSeries_PaintsOnlyNearTheLine)
{
    SparklineWidget widget;
    widget.resize(200, 100);
    widget.setSamples(Samples(60, 100.0f)); // A line along the top edge.
    const QRect plot = widget.plotArea().toAlignedRect();
    const QRect lowerHalf(plot.left(), plot.center().y(), plot.width(), plot.height() / 2);

    EXPECT_TRUE(hasPaintedPixel(renderWidget(widget), lowerHalf)); // Gradient fill.

    widget.setSeriesFilled(0, false);

    EXPECT_FALSE(hasPaintedPixel(renderWidget(widget), lowerHalf));
}

// ---------------------------------------------------------------------------
// upperEnvelope
// ---------------------------------------------------------------------------

TEST(SparklineEnvelope, KeepsHighestPointPerPixelColumn)
{
    const QPolygonF line{QPointF{0.2, 40.0}, QPointF{0.6, 10.0}, QPointF{1.1, 30.0}, QPointF{1.9, 45.0}};

    const QPolygonF envelope = upperEnvelope(line, kArea);

    ASSERT_EQ(envelope.size(), 2);
    EXPECT_DOUBLE_EQ(envelope[0].x(), 0.2);
    EXPECT_DOUBLE_EQ(envelope[0].y(), 10.0);
    EXPECT_DOUBLE_EQ(envelope[1].x(), 1.1);
    EXPECT_DOUBLE_EQ(envelope[1].y(), 30.0);
}
