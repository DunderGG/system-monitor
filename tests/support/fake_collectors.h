#pragma once

#include <optional>
#include <utility>

#include "domain/cpu_sample.h"
#include "domain/memory_sample.h"
#include "monitoring/collector.h"
#include "support/snapshots.h"

namespace sysmon::tests
{

/** A CPU collector that returns the same sample, by default populatedSnapshot()'s, from every collect(). */
class FakeCpuCollector : public monitoring::ICpuCollector
{
public:
    explicit FakeCpuCollector(std::optional<domain::CpuSample> sample = populatedSnapshot().cpu)
        : m_sample(std::move(sample))
    {}

    [[nodiscard]] std::optional<domain::CpuSample> collect() override
    {
        return m_sample;
    }

private:
    const std::optional<domain::CpuSample> m_sample;
};

/** A memory collector that returns the same sample, by default populatedSnapshot()'s, from every collect(). */
class FakeMemoryCollector : public monitoring::IMemoryCollector
{
public:
    explicit FakeMemoryCollector(std::optional<domain::MemorySample> sample = populatedSnapshot().memory)
        : m_sample(std::move(sample))
    {}

    [[nodiscard]] std::optional<domain::MemorySample> collect() override
    {
        return m_sample;
    }

private:
    const std::optional<domain::MemorySample> m_sample;
};

static_assert(monitoring::Collector<FakeCpuCollector, std::optional<domain::CpuSample>>);
static_assert(monitoring::Collector<FakeMemoryCollector, std::optional<domain::MemorySample>>);

} // namespace sysmon::tests
