#include "monitoring/synthetic_cpu_collector.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace sysmon::monitoring
{

SyntheticCpuCollector::SyntheticCpuCollector(int coreCount, float baseUsagePercent)
    : m_coreCount(std::max(1, coreCount)), m_baseUsagePercent(std::clamp(baseUsagePercent, 0.0f, 100.0f))
{}

domain::CpuSample SyntheticCpuCollector::collect()
{
    const float radians = static_cast<float>(m_step) * 0.1f;
    const float rawTotal = m_baseUsagePercent + 20.0f * std::sin(radians) + 5.0f * std::sin(radians * 2.3f);
    const float totalUsage = std::clamp(rawTotal, 0.0f, 100.0f);

    std::vector<float> coreUsages;
    coreUsages.reserve(static_cast<std::size_t>(m_coreCount));

    for (int i = 0; i < m_coreCount; ++i) {
        const float phase = radians + static_cast<float>(i) * 0.7f;
        const float rawCore = totalUsage + 12.0f * std::sin(phase);
        coreUsages.push_back(std::clamp(rawCore, 0.0f, 100.0f));
    }

    ++m_step;

    return domain::CpuSample{
        .totalUsagePercent = totalUsage,
        .coreUsagePercents = std::move(coreUsages),
        .coreCount = m_coreCount,
    };
}

void SyntheticCpuCollector::setCoreCount(int coreCount)
{
    assert(coreCount >= 1 && "Core count must be at least 1");
    m_coreCount = std::max(1, coreCount);
}

int SyntheticCpuCollector::coreCount() const
{
    return m_coreCount;
}

void SyntheticCpuCollector::setBaseUsagePercent(float percent)
{
    m_baseUsagePercent = std::clamp(percent, 0.0f, 100.0f);
}

float SyntheticCpuCollector::baseUsagePercent() const
{
    return m_baseUsagePercent;
}

void SyntheticCpuCollector::setStep(std::size_t step)
{
    m_step = step;
}

std::size_t SyntheticCpuCollector::step() const
{
    return m_step;
}

} // namespace sysmon::monitoring
