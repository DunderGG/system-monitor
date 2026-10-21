#include <chrono>
#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <QColor>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QString>

#include "support/sparkline_samples.h"
#include "ui/charts/sparkline_widget.h"

using namespace verdandi::ui::charts;
using verdandi::tests::kChartArea;
using verdandi::tests::sineSamples;

namespace
{

using Samples = verdandi::tests::SparklineSamples;

// Plain digits, so label widths depend only on the number of digits.
QString wholeNumber(float value)
{
    return QString::number(std::llround(value));
}

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

} // namespace

// ---------------------------------------------------------------------------
// Buffer sizes from the roadmap (60, 300, 1800 samples)
// ---------------------------------------------------------------------------

class SparklineBufferSize : public ::testing::TestWithParam<std::size_t>
{};

TEST_P(SparklineBufferSize, FullSyntheticHistory_ProducesOneSegmentAcrossWidth)
{
    const std::size_t size = GetParam();
    const Samples samples = sineSamples(size);

    const auto segments = sparklineSegments(samples, size, kChartArea, kPercentRange);

    // Up to 100 samples, one point each; beyond the 100-pixel width, at most
    // two per pixel column (101 columns including the right edge).
    ASSERT_EQ(segments.size(), 1u);
    const auto pointCount = static_cast<std::size_t>(segments[0].size());
    if (size <= 100) {
        EXPECT_EQ(pointCount, size);
    }
    EXPECT_LE(pointCount, 2u * 101u);
    EXPECT_GE(segments[0].front().x(), kChartArea.left());
    EXPECT_LT(segments[0].front().x(), kChartArea.left() + 1.0);
    EXPECT_DOUBLE_EQ(segments[0].back().x(), kChartArea.right());
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

    EXPECT_EQ(verticalGridLines(size, size - 1, kChartArea).size(), 10u);
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

TEST(SparklineWidget, AnnotationFormatter_DefaultsToValueFormatter)
{
    SparklineWidget widget;
    widget.setValueFormatter(wholeNumber);
    EXPECT_EQ(widget.formatAnnotationValue(7.5f), "8");

    widget.setAnnotationFormatter([](float value) { return QString::number(value, 'f', 1); });
    EXPECT_EQ(widget.formatAnnotationValue(7.5f), "7.5");
    EXPECT_EQ(widget.formatValue(7.5f), "8");

    widget.setAnnotationFormatter({});
    EXPECT_EQ(widget.formatAnnotationValue(7.5f), "8");
}

TEST(SparklineWidget, ExtremesText_SingleSeriesUsesAnnotationFormatter)
{
    SparklineWidget widget;
    widget.setValueFormatter(wholeNumber);
    widget.setAnnotationFormatter([](float value) { return QString::number(value, 'f', 1); });
    EXPECT_TRUE(widget.extremesText().isEmpty());

    widget.setSamples(Samples{12.5f, std::nullopt, 7.26f});

    EXPECT_EQ(widget.extremesText(), "Min 7.3  Max 12.5");
}

TEST(SparklineWidget, ExtremesText_LegendNamesPrimarySeries)
{
    SparklineWidget widget;
    widget.setValueFormatter(wholeNumber);
    widget.setSeriesCount(2);
    widget.setSeriesLabel(0, "Receive");
    widget.setSeriesLabel(1, "Send");
    widget.setSeriesSamples(0, Samples{1.0f, 4.0f});
    widget.setSeriesSamples(1, Samples{90.0f, 99.0f});

    EXPECT_EQ(widget.extremesText(), "Receive min 1  max 4");
}

TEST(SparklineWidget, SeriesColorSlot_UsesThatSlotsDefaultColor)
{
    SparklineWidget widget;
    widget.setSeriesCount(3);
    const QColor slotOneColor = widget.seriesColor(1);

    widget.setSeriesColorSlot(2, 1);

    EXPECT_EQ(widget.seriesColor(2), slotOneColor);
    widget.setSeriesColor(2, Qt::black);
    EXPECT_EQ(widget.seriesColor(2), QColor(Qt::black));
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

TEST(SparklineWidget, AutoRangeWithoutTemplates_PlotMovesWithLabelWidth)
{
    SparklineWidget widget;
    widget.resize(600, 150);
    widget.setAutoRange();
    widget.setValueFormatter(wholeNumber);
    widget.setAxisLabelsVisible(true);
    widget.setSamples(Samples{9.0f});
    const qreal narrowLeft = widget.plotArea().left();

    widget.setSamples(Samples{9'000'000.0f});

    EXPECT_GT(widget.plotArea().left(), narrowLeft);
}

TEST(SparklineWidget, AxisLabelTemplates_PlotStaysWhenAutoRangeChanges)
{
    SparklineWidget widget;
    widget.resize(600, 150);
    widget.setAutoRange();
    widget.setValueFormatter(wholeNumber);
    widget.setAxisLabelsVisible(true);
    widget.setAxisLabelTemplates({"88888888", "8888"});
    widget.setSamples(Samples{9.0f});
    const qreal left = widget.plotArea().left();

    widget.setSamples(Samples{9'000'000.0f});
    EXPECT_DOUBLE_EQ(widget.plotArea().left(), left);
    widget.setSamples(Samples{900.0f});
    EXPECT_DOUBLE_EQ(widget.plotArea().left(), left);
}

TEST(SparklineWidget, LabelWiderThanTemplates_GutterStillFitsIt)
{
    SparklineWidget widget;
    widget.resize(600, 150);
    widget.setAutoRange();
    widget.setValueFormatter(wholeNumber);
    widget.setAxisLabelsVisible(true);
    widget.setAxisLabelTemplates({"8"});
    widget.setSamples(Samples{9.0f});
    const qreal left = widget.plotArea().left();

    widget.setSamples(Samples{9'000'000.0f});

    EXPECT_GT(widget.plotArea().left(), left);
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
