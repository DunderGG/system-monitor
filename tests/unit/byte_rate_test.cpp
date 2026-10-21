#include <gtest/gtest.h>

#include "platform/windows/byte_rate.h"

using verdandi::platform::calculateByteRate;

TEST(ByteRate, CalculateByteRate_CounterGrew_BytesPerSecondRounded)
{
    EXPECT_EQ(calculateByteRate(1000, 5000, 2.0), 2000u);
    EXPECT_EQ(calculateByteRate(0, 3, 2.0), 2u);
}

TEST(ByteRate, CalculateByteRate_CounterUnchanged_Zero)
{
    EXPECT_EQ(calculateByteRate(1000, 1000, 1.0), 0u);
}

TEST(ByteRate, CalculateByteRate_CounterWentBackwards_Nullopt)
{
    EXPECT_FALSE(calculateByteRate(1000, 999, 1.0).has_value());
}

TEST(ByteRate, CalculateByteRate_NoElapsedTime_Nullopt)
{
    EXPECT_FALSE(calculateByteRate(1000, 2000, 0.0).has_value());
    EXPECT_FALSE(calculateByteRate(1000, 2000, -1.0).has_value());
}
