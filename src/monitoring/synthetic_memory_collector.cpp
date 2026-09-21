#include "monitoring/synthetic_memory_collector.h"

#include <algorithm>
#include <cmath>

namespace sysmon::monitoring
{

SyntheticMemoryCollector::SyntheticMemoryCollector(
    uint64_t totalBytes,
    uint64_t commitLimit,
    float    baseUsagePercent)
    : m_totalBytes(totalBytes)
    , m_commitLimit(commitLimit)
    , m_baseUsagePercent(std::clamp(baseUsagePercent, 0.0f, 100.0f))
{
}

domain::MemorySample SyntheticMemoryCollector::collect()
{
    const float radians = static_cast<float>(m_step) * 0.05f;
    const float rawUsage = m_baseUsagePercent
        + 15.0f * std::sin(radians)
        + 3.0f * std::cos(radians * 1.7f);
    const float usagePercent = std::clamp(rawUsage, 1.0f, 99.0f);

    const auto usedBytes = static_cast<uint64_t>(
        static_cast<double>(m_totalBytes) * (static_cast<double>(usagePercent) / 100.0));
    const uint64_t availableBytes = (m_totalBytes > usedBytes) ? (m_totalBytes - usedBytes) : 0ULL;

    const auto commitCurrent = static_cast<uint64_t>(
        static_cast<double>(m_commitLimit) * (static_cast<double>(usagePercent * 0.85f) / 100.0));

    ++m_step;

    return domain::MemorySample{
        .totalBytes = m_totalBytes,
        .availableBytes = availableBytes,
        .usagePercent = usagePercent,
        .commitLimit = m_commitLimit,
        .commitCurrent = commitCurrent,
    };
}

void SyntheticMemoryCollector::setTotalBytes(uint64_t bytes)
{
    m_totalBytes = bytes;
}

uint64_t SyntheticMemoryCollector::totalBytes() const
{
    return m_totalBytes;
}

void SyntheticMemoryCollector::setCommitLimit(uint64_t bytes)
{
    m_commitLimit = bytes;
}

uint64_t SyntheticMemoryCollector::commitLimit() const
{
    return m_commitLimit;
}

void SyntheticMemoryCollector::setBaseUsagePercent(float percent)
{
    m_baseUsagePercent = std::clamp(percent, 0.0f, 100.0f);
}

float SyntheticMemoryCollector::baseUsagePercent() const
{
    return m_baseUsagePercent;
}

void SyntheticMemoryCollector::setStep(std::size_t step)
{
    m_step = step;
}

std::size_t SyntheticMemoryCollector::step() const
{
    return m_step;
}

} // namespace sysmon::monitoring

