#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "domain/disk_sample.h"
#include "monitoring/collector.h"

namespace sysmon::platform
{

/**
 * Raw disk space metrics in bytes.
 */
struct DiskSpaceData
{
    uint64_t totalBytes{0};
    uint64_t freeBytes{0};
};

/**
 * Calculates a DiskSample from volume name, total bytes, and free bytes.
 * Pure function with no OS dependencies for deterministic unit testing.
 */
[[nodiscard]] domain::DiskSample calculateDiskSample(
    const std::string &volumeName,
    uint64_t totalBytes,
    uint64_t freeBytes);

/**
 * Disk space metrics collector for Windows fixed drives using GetDiskFreeSpaceExW.
 */
class DiskCollector : public monitoring::IDiskCollector
{
public:
    using FixedDriveEnumerator = std::function<std::vector<std::string>()>;
    using DiskSpaceReader = std::function<bool(const std::string &volumeName, DiskSpaceData &data)>;

    explicit DiskCollector();
    DiskCollector(FixedDriveEnumerator enumerator, DiskSpaceReader reader);
    ~DiskCollector() override = default;

    [[nodiscard]] std::vector<domain::DiskSample> collect() override;

private:
    FixedDriveEnumerator m_enumerator;
    DiskSpaceReader m_reader;
};

} // namespace sysmon::platform

