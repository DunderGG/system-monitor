#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "monitoring/ring_buffer.h"

using sysmon::monitoring::RingBuffer;

TEST(RingBuffer, DefaultState_IsEmptyAndCorrectCapacity)
{
    const RingBuffer<int> buffer(5);

    EXPECT_EQ(buffer.capacity(), 5u);
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_TRUE(buffer.samples().empty());
}

TEST(RingBuffer, PushBelowCapacity_RetainsAllInOrder)
{
    RingBuffer<int> buffer(5);

    buffer.push(10);
    buffer.push(20);

    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_FALSE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    ASSERT_EQ(buffer.samples().size(), 2u);
    EXPECT_EQ(buffer.samples()[0], 10);
    EXPECT_EQ(buffer.samples()[1], 20);
    EXPECT_EQ(buffer[0], 10);
    EXPECT_EQ(buffer[1], 20);
    EXPECT_EQ(buffer.front(), 10);
    EXPECT_EQ(buffer.back(), 20);
}

TEST(RingBuffer, PushToExactCapacity_IsFullAndMaintainsOrder)
{
    RingBuffer<int> buffer(3);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_TRUE(buffer.full());
    EXPECT_FALSE(buffer.empty());
    ASSERT_EQ(buffer.samples().size(), 3u);
    EXPECT_EQ(buffer[0], 1);
    EXPECT_EQ(buffer[1], 2);
    EXPECT_EQ(buffer[2], 3);
    EXPECT_EQ(buffer.front(), 1);
    EXPECT_EQ(buffer.back(), 3);
}

TEST(RingBuffer, PushBeyondCapacity_OverwritesOldest)
{
    RingBuffer<int> buffer(3);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4); // Overwrites 1

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_TRUE(buffer.full());
    ASSERT_EQ(buffer.samples().size(), 3u);
    EXPECT_EQ(buffer.samples()[0], 2);
    EXPECT_EQ(buffer.samples()[1], 3);
    EXPECT_EQ(buffer.samples()[2], 4);
    EXPECT_EQ(buffer.front(), 2);
    EXPECT_EQ(buffer.back(), 4);
}

TEST(RingBuffer, MultipleWraparounds_MaintainsCorrectOrder)
{
    RingBuffer<int> buffer(3);

    for (int i = 1; i <= 10; ++i) {
        buffer.push(i);
    }

    EXPECT_EQ(buffer.size(), 3u);
    EXPECT_TRUE(buffer.full());
    ASSERT_EQ(buffer.samples().size(), 3u);
    EXPECT_EQ(buffer[0], 8);
    EXPECT_EQ(buffer[1], 9);
    EXPECT_EQ(buffer[2], 10);
    EXPECT_EQ(buffer.front(), 8);
    EXPECT_EQ(buffer.back(), 10);
}

TEST(RingBuffer, Clear_ResetsSizeAndEmpty)
{
    RingBuffer<int> buffer(4);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.clear();

    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_TRUE(buffer.empty());
    EXPECT_FALSE(buffer.full());
    EXPECT_EQ(buffer.capacity(), 4u);
    EXPECT_TRUE(buffer.samples().empty());

    // Can push again after clear
    buffer.push(42);
    EXPECT_EQ(buffer.size(), 1u);
    EXPECT_EQ(buffer.front(), 42);
}

TEST(RingBuffer, RangeForIteration_VisitsAllSamplesInOrder)
{
    RingBuffer<int> buffer(4);

    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);
    buffer.push(5); // Oldest 1 is overwritten; remaining: 2, 3, 4, 5

    std::vector<int> iterated;
    for (const int sample : buffer) {
        iterated.push_back(sample);
    }

    const std::vector<int> expected = {2, 3, 4, 5};
    EXPECT_EQ(iterated, expected);
}

TEST(RingBuffer, MoveSemantics_PushesMovedObjects)
{
    RingBuffer<std::string> buffer(2);

    std::string first = "first_string";
    std::string second = "second_string";

    buffer.push(std::move(first));
    buffer.push(std::move(second));

    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], "first_string");
    EXPECT_EQ(buffer[1], "second_string");
}

TEST(RingBuffer, CapacityOne_OverwritesCorrectly)
{
    RingBuffer<int> buffer(1);

    buffer.push(100);
    EXPECT_EQ(buffer.size(), 1u);
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.front(), 100);

    buffer.push(200);
    EXPECT_EQ(buffer.size(), 1u);
    EXPECT_TRUE(buffer.full());
    EXPECT_EQ(buffer.front(), 200);
    EXPECT_EQ(buffer.back(), 200);
}

