#pragma once

#include <cstdint>
#include <functional>
#include <optional>
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
 * Returns std::nullopt when totalBytes is zero, which is never a valid reading;
 * collect() skips such volumes just as it skips volumes whose query fails.
 * Pure function with no OS dependencies for deterministic unit testing.
 */
[[nodiscard]] std::optional<domain::DiskSample> calculateDiskSample(const std::string& volumeName, uint64_t totalBytes,
                                                                    uint64_t freeBytes);

/**
 * Disk space metrics collector for Windows fixed drives using GetDiskFreeSpaceExW.
 *
 * collect() returns std::nullopt when the drives cannot be enumerated, or when
 * fixed drives exist but none of them could be read. Volumes whose query fails
 * or that report zero capacity are skipped.
 */
class DiskCollector : public monitoring::IDiskCollector
{
public:
    /** Returns the root paths of all fixed drives (e.g. "C:\\"), or std::nullopt on failure. */
    using FixedDriveEnumerator = std::function<std::optional<std::vector<std::string>>()>;
    using DiskSpaceReader = std::function<bool(const std::string& volumeName, DiskSpaceData& data)>;

    explicit DiskCollector();
    DiskCollector(FixedDriveEnumerator enumerator, DiskSpaceReader reader);
    ~DiskCollector() override = default;

    [[nodiscard]] std::optional<std::vector<domain::DiskSample>> collect() override;

private:
    FixedDriveEnumerator m_enumerator;
    DiskSpaceReader m_reader;
};

} // namespace sysmon::platform
