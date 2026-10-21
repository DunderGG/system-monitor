#include <gtest/gtest.h>

#include "platform/windows/system_process_information_reader.h"

using namespace verdandi::platform;

TEST(SystemProcessInformationReaderIntegration, RealHost_SmallInitialBuffer_GrowsAndReads)
{
    // A 1 KiB buffer cannot hold any real process list, so the first read must
    // go through the resizing loop.
    SystemProcessInformationReader reader(resolveSystemProcessInformationQuery(), 1024);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    EXPECT_GT(bytes->size(), 1024u);
    EXPECT_GE(reader.bufferSize(), bytes->size());
}

TEST(SystemProcessInformationReaderIntegration, RealHost_RepeatedReads_Succeed)
{
    SystemProcessInformationReader reader;

    const auto first = reader.read();
    ASSERT_TRUE(first.has_value());
    EXPECT_FALSE(first->empty());

    const auto second = reader.read();
    ASSERT_TRUE(second.has_value());
    EXPECT_FALSE(second->empty());
}
