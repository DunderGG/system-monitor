#pragma once

#include <cstddef>

#include "domain/cpu_sample.h"
#include "monitoring/collector.h"

namespace sysmon::monitoring
{

/**
 * Generates synthetic CPU metrics with realistic variation (sinusoidal + harmonics).
 * Useful for validating the monitoring pipeline, UI development, and unit testing.
 */
class SyntheticCpuCollector : public ICpuCollector
{
public:
    /**
     * Constructs a synthetic CPU collector.
     * @param coreCount Number of logical cores to simulate (must be >= 1).
     * @param baseUsagePercent Base CPU utilization around which values oscillate.
     */
    explicit SyntheticCpuCollector(int coreCount = 8, float baseUsagePercent = 35.0f);
    ~SyntheticCpuCollector() override = default;

    [[nodiscard]] domain::CpuSample collect() override;

    void setCoreCount(int coreCount);
    [[nodiscard]] int coreCount() const;

    void setBaseUsagePercent(float percent);
    [[nodiscard]] float baseUsagePercent() const;

    void setStep(std::size_t step);
    [[nodiscard]] std::size_t step() const;

private:
    int         m_coreCount{8};
    float       m_baseUsagePercent{35.0f};
    std::size_t m_step{0};
};

} // namespace sysmon::monitoring

