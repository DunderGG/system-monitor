#include <cmath>
#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QRegion>

#include "ui/charts/sparkline_widget.h"

using namespace sysmon::ui::charts;

namespace
{

using Samples = std::vector<std::optional<float>>;

const QRectF kArea{0.0, 0.0, 100.0, 50.0};
constexpr YRange kPercent{.min = 0.0f, .max = 100.0f};

// True if any pixel in the image is not fully transparent.
bool hasPaintedPixel(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(image.pixel(x, y)) > 0) {
                return true;
            }
        }
    }
    return false;
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
    const auto &line = segments[0];
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

TEST(SparklineSegments, CapacityOne_PlacesSampleAtRightEdge)
{
    const auto segments = sparklineSegments(Samples{50.0f}, 1, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_DOUBLE_EQ(segments[0][0].x(), 100.0);
}

// ---------------------------------------------------------------------------
// Buffer sizes from the roadmap (60, 300, 1800 samples)
// ---------------------------------------------------------------------------

class SparklineBufferSize : public ::testing::TestWithParam<std::size_t>
{
};

TEST_P(SparklineBufferSize, FullSyntheticHistory_ProducesOneSegmentAcrossWidth)
{
    const std::size_t size = GetParam();
    Samples samples;
    samples.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        samples.push_back(50.0f + 40.0f * static_cast<float>(std::sin(static_cast<double>(i) * 0.1)));
    }

    const auto segments = sparklineSegments(samples, size, kArea, kPercent);

    ASSERT_EQ(segments.size(), 1u);
    EXPECT_EQ(static_cast<std::size_t>(segments[0].size()), size);
    EXPECT_DOUBLE_EQ(segments[0].front().x(), kArea.left());
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

    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image, QPoint(), QRegion(), QWidget::DrawChildren); // Skip the window background.

    EXPECT_TRUE(hasPaintedPixel(image));
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

TEST(SparklineWidget, AutoRange_FollowsSamples)
{
    SparklineWidget widget;
    widget.setAutoRange();

    widget.setSamples(Samples{200.0f, 400.0f});

    EXPECT_TRUE(widget.isAutoRange());
    EXPECT_FLOAT_EQ(widget.effectiveRange().max, 440.0f);
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

TEST(SparklineWidget, NoSamples_PaintsNothing)
{
    SparklineWidget widget;
    widget.resize(160, 36);

    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image, QPoint(), QRegion(), QWidget::DrawChildren); // Skip the window background.

    EXPECT_FALSE(hasPaintedPixel(image));
}
