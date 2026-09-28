#include "platform/windows/disk_collector.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <Windows.h>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

namespace
{

std::optional<std::vector<std::string>> enumerateFixedDrives(RepeatedFailureLog &failureLog)
{
    const DWORD driveMask = ::GetLogicalDrives();
    if (driveMask == 0) {
        failureLog.failure(spdlog::level::err, "GetLogicalDrives failed with error code: {}", ::GetLastError());
        return std::nullopt;
    }
    failureLog.success();

    std::vector<std::string> fixedDrives;
    for (int i = 0; i < 26; ++i) {
        if (driveMask & (1 << i)) {
            const wchar_t rootW[] = { static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0' };
            const UINT driveType = ::GetDriveTypeW(rootW);
            if (driveType == DRIVE_FIXED) {
                const char rootA[] = { static_cast<char>('A' + i), ':', '\\', '\0' };
                fixedDrives.emplace_back(rootA);
            }
        }
    }
    return fixedDrives;
}

bool readDiskSpace(const std::string &volumeName, DiskSpaceData &data, RepeatedFailureLog &failureLog)
{
    const std::wstring volumeW(volumeName.begin(), volumeName.end());
    ULARGE_INTEGER freeBytesAvailable{};
    ULARGE_INTEGER totalNumberOfBytes{};
    ULARGE_INTEGER totalNumberOfFreeBytes{};

    if (!::GetDiskFreeSpaceExW(
            volumeW.c_str(),
            &freeBytesAvailable,
            &totalNumberOfBytes,
            &totalNumberOfFreeBytes)) {
        const DWORD error = ::GetLastError();
        failureLog.failure(spdlog::level::warn, "GetDiskFreeSpaceExW failed for volume '{}' with error code: {}",
                           volumeName, error);
        return false;
    }
    failureLog.success();

    data.totalBytes = totalNumberOfBytes.QuadPart;
    data.freeBytes = freeBytesAvailable.QuadPart;
    return true;
}

} // namespace

std::optional<domain::DiskSample> calculateDiskSample(
    const std::string &volumeName,
    uint64_t totalBytes,
    uint64_t freeBytes)
{
    if (totalBytes == 0) {
        return std::nullopt;
    }

    const uint64_t validFreeBytes = std::min(freeBytes, totalBytes);
    const uint64_t usedBytes = totalBytes - validFreeBytes;
    const float usagePercent =
        std::clamp(static_cast<float>(usedBytes) * 100.0f / static_cast<float>(totalBytes), 0.0f, 100.0f);

    return domain::DiskSample{
        .volumeName = volumeName,
        .totalBytes = totalBytes,
        .freeBytes = validFreeBytes,
        .usagePercent = usagePercent,
    };
}

DiskCollector::DiskCollector()
    : m_enumerator([failureLog = RepeatedFailureLog{"GetLogicalDrives"}]() mutable {
          return enumerateFixedDrives(failureLog);
      }),
      // One failure log per volume, so a locked volume does not hide another's recovery.
      m_reader([failureLogs = std::unordered_map<std::string, RepeatedFailureLog>{}](
                   const std::string &volumeName, DiskSpaceData &data) mutable {
          auto &failureLog =
              failureLogs.try_emplace(volumeName, "GetDiskFreeSpaceExW(" + volumeName + ")").first->second;
          return readDiskSpace(volumeName, data, failureLog);
      })
{}

DiskCollector::DiskCollector(FixedDriveEnumerator enumerator, DiskSpaceReader reader)
    : m_enumerator(std::move(enumerator)),
      m_reader(std::move(reader))
{}

std::optional<std::vector<domain::DiskSample>> DiskCollector::collect()
{
    if (!m_enumerator || !m_reader) {
        return std::nullopt;
    }

    const auto drives = m_enumerator();
    if (!drives) {
        return std::nullopt;
    }

    std::vector<domain::DiskSample> samples;
    samples.reserve(drives->size());
    bool hasFailedRead = false;

    for (const auto &drive : *drives) {
        DiskSpaceData data{};
        if (!m_reader(drive, data)) {
            hasFailedRead = true;
            continue;
        }
        if (auto sample = calculateDiskSample(drive, data.totalBytes, data.freeBytes)) {
            samples.push_back(std::move(*sample));
        }
    }

    // Fixed drives exist but none could be read: that is missing data, not "no volumes".
    if (samples.empty() && hasFailedRead) {
        return std::nullopt;
    }
    return samples;
}

} // namespace sysmon::platform

