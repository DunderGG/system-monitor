#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "domain/cpu_sample.h"
#include "domain/memory_sample.h"
#include "domain/process_info.h"
#include "monitoring/collector.h"
#include "support/snapshots.h"

namespace verdandi::tests
{

/**
 * A CPU collector that returns the same sample, by default populatedSnapshot()'s,
 * from every collect(), and so has no baselines to reset.
 */
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

    void resetBaselines() override {}

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

/**
 * A process collector that returns one process, whose PID is the number of
 * the call, and records how often and on which thread it was last called.
 */
class FakeProcessCollector : public monitoring::IProcessCollector
{
public:
    [[nodiscard]] std::optional<std::vector<domain::ProcessInfo>> collect() override
    {
        {
            const std::lock_guard lock(m_mutex);
            m_lastThread = std::this_thread::get_id();
        }
        const int call = ++m_calls;
        return std::vector<domain::ProcessInfo>{
            domain::ProcessInfo{.pid = static_cast<uint32_t>(call), .imageName = "app.exe"}};
    }

    void resetBaselines() override {}

    [[nodiscard]] int calls() const
    {
        return m_calls.load();
    }

    [[nodiscard]] std::thread::id lastThread() const
    {
        const std::lock_guard lock(m_mutex);
        return m_lastThread;
    }

private:
    std::atomic<int> m_calls{0};
    mutable std::mutex m_mutex;
    std::thread::id m_lastThread;
};

} // namespace verdandi::tests
