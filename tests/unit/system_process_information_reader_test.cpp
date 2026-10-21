#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "platform/windows/system_process_information_reader.h"

using namespace verdandi::platform;

namespace
{

constexpr int32_t kStatusAccessDenied = static_cast<int32_t>(0xC0000022);

// Fakes a process list of requiredSize bytes: a smaller buffer gets
// mismatchStatus and the required size, a large enough one is filled with a
// byte pattern.
class FakeProcessListQuery
{
public:
    explicit FakeProcessListQuery(std::size_t requiredSize, int32_t mismatchStatus = ntstatus::kInfoLengthMismatch)
        : m_requiredSize(requiredSize), m_mismatchStatus(mismatchStatus)
    {}

    int32_t operator()(std::span<std::byte> buffer, uint32_t& returnLength)
    {
        ++calls;
        bufferSizes.push_back(buffer.size());
        returnLength = static_cast<uint32_t>(m_requiredSize);
        if (buffer.size() < m_requiredSize) {
            return m_mismatchStatus;
        }
        for (std::size_t i = 0; i < m_requiredSize; ++i) {
            buffer[i] = static_cast<std::byte>(i % 251);
        }
        return ntstatus::kSuccess;
    }

    int calls{0};
    std::vector<std::size_t> bufferSizes;

private:
    std::size_t m_requiredSize;
    int32_t m_mismatchStatus;
};

} // namespace

TEST(SystemProcessInformationReader, Read_BufferLargeEnough_ReturnsWrittenBytesOnly)
{
    FakeProcessListQuery query(100);
    SystemProcessInformationReader reader(
        [&query](std::span<std::byte> buffer, uint32_t& length) { return query(buffer, length); }, 1024);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    ASSERT_EQ(bytes->size(), 100u);
    EXPECT_EQ((*bytes)[99], static_cast<std::byte>(99));
    EXPECT_EQ(query.calls, 1);
}

TEST(SystemProcessInformationReader, Read_LengthMismatch_GrowsPastReportedSizeAndRetries)
{
    FakeProcessListQuery query(1000);
    SystemProcessInformationReader reader(
        [&query](std::span<std::byte> buffer, uint32_t& length) { return query(buffer, length); }, 16);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes->size(), 1000u);
    EXPECT_EQ(query.calls, 2);
    // Headroom above the reported size for processes created before the retry.
    EXPECT_GT(reader.bufferSize(), 1000u);
}

TEST(SystemProcessInformationReader, Read_BufferTooSmallStatus_AlsoRetries)
{
    FakeProcessListQuery query(1000, ntstatus::kBufferTooSmall);
    SystemProcessInformationReader reader(
        [&query](std::span<std::byte> buffer, uint32_t& length) { return query(buffer, length); }, 16);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes->size(), 1000u);
}

TEST(SystemProcessInformationReader, Read_MismatchWithoutReportedSize_DoublesBuffer)
{
    int calls = 0;
    std::vector<std::size_t> bufferSizes;
    SystemProcessInformationReader reader(
        [&](std::span<std::byte> buffer, uint32_t& length) {
            ++calls;
            bufferSizes.push_back(buffer.size());
            length = 0;
            return buffer.size() < 64 ? ntstatus::kInfoLengthMismatch : ntstatus::kSuccess;
        },
        16);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    // Doubled, without the headroom a reported size gets.
    EXPECT_EQ(bufferSizes, (std::vector<std::size_t>{16, 32, 64}));
}

TEST(SystemProcessInformationReader, Read_SuccessWithoutReportedLength_ReturnsWholeBuffer)
{
    SystemProcessInformationReader reader(
        [](std::span<std::byte> /*buffer*/, uint32_t& length) {
            length = 0;
            return ntstatus::kSuccess;
        },
        64);

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(bytes->size(), 64u);
}

TEST(SystemProcessInformationReader, Read_StillTooSmallAfterMaxAttempts_ReturnsNullopt)
{
    int calls = 0;
    SystemProcessInformationReader reader(
        [&calls](std::span<std::byte> buffer, uint32_t& length) {
            // The list always grows faster than the buffer.
            ++calls;
            length = static_cast<uint32_t>(buffer.size() * 4);
            return ntstatus::kInfoLengthMismatch;
        },
        16);

    const auto bytes = reader.read();

    EXPECT_FALSE(bytes.has_value());
    EXPECT_EQ(calls, SystemProcessInformationReader::kMaxAttempts);
}

TEST(SystemProcessInformationReader, Read_OtherFailureStatus_ReturnsNulloptWithoutRetry)
{
    int calls = 0;
    SystemProcessInformationReader reader(
        [&calls](std::span<std::byte> /*buffer*/, uint32_t& length) {
            ++calls;
            length = 0;
            return kStatusAccessDenied;
        },
        64);

    const auto bytes = reader.read();

    EXPECT_FALSE(bytes.has_value());
    EXPECT_EQ(calls, 1);
}

TEST(SystemProcessInformationReader, Read_ReportedSizeAboveLimit_ReturnsNulloptWithoutGrowing)
{
    SystemProcessInformationReader reader(
        [](std::span<std::byte> /*buffer*/, uint32_t& length) {
            length = static_cast<uint32_t>(SystemProcessInformationReader::kMaxBufferSize + 1);
            return ntstatus::kInfoLengthMismatch;
        },
        64);

    const auto bytes = reader.read();

    EXPECT_FALSE(bytes.has_value());
    EXPECT_EQ(reader.bufferSize(), 64u);
}

TEST(SystemProcessInformationReader, Read_SecondRead_ReusesGrownBuffer)
{
    FakeProcessListQuery query(1000);
    SystemProcessInformationReader reader(
        [&query](std::span<std::byte> buffer, uint32_t& length) { return query(buffer, length); }, 16);
    ASSERT_TRUE(reader.read().has_value());
    const int callsAfterFirstRead = query.calls;

    const auto bytes = reader.read();

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(query.calls, callsAfterFirstRead + 1);
}

TEST(SystemProcessInformationReader, Read_NoQuery_ReturnsNullopt)
{
    SystemProcessInformationReader reader(nullptr, 64);

    EXPECT_FALSE(reader.read().has_value());
}
