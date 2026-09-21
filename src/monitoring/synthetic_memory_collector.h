#pragma once

#include <cstddef>
#include <cstdint>

#include "domain/memory_sample.h"
#include "monitoring/collector.h"

namespace sysmon::monitoring
{

/**
 * Generates synthetic memory metrics with realistic values and smooth variation.
 * Useful for validating the monitoring pipeline, UI development, and unit testing.
 */
class SyntheticMemoryCollector : public IMemoryCollector
{
public:
    /**
     * Constructs a synthetic memory collector.
     * @param totalBytes Total physical memory to simulate in bytes (defaults to 32 GiB).
     * @param commitLimit Total commit limit to simulate in bytes (defaults to 40 GiB).
     * @param baseUsagePercent Base physical memory usage percent.
     */
    explicit SyntheticMemoryCollector(
        uint64_t totalBytes = 34'359'738'368ULL,
        uint64_t commitLimit = 42'949'672'960ULL,
        float    baseUsagePercent = 45.0f);
    ~SyntheticMemoryCollector() override = default;

    [[nodiscard]] domain::MemorySample collect() override;

    void setTotalBytes(uint64_t bytes);
    [[nodiscard]] uint64_t totalBytes() const;

    void setCommitLimit(uint64_t bytes);
    [[nodiscard]] uint64_t commitLimit() const;

    void setBaseUsagePercent(float percent);
    [[nodiscard]] float baseUsagePercent() const;

    void setStep(std::size_t step);
    [[nodiscard]] std::size_t step() const;

private:
    uint64_t    m_totalBytes{34'359'738'368ULL};
    uint64_t    m_commitLimit{42'949'672'960ULL};
    float       m_baseUsagePercent{45.0f};
    std::size_t m_step{0};
};

} // namespace sysmon::monitoring

