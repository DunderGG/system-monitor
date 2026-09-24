#include "platform/windows/disk_collector.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <Windows.h>

namespace sysmon::platform
{

namespace
{

std::vector<std::string> enumerateFixedDrives()
{
    std::vector<std::string> fixedDrives;
    const DWORD driveMask = ::GetLogicalDrives();
    if (driveMask == 0) {
        spdlog::error("GetLogicalDrives failed with error code: {}", ::GetLastError());
        return fixedDrives;
    }

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

bool readDiskSpace(const std::string &volumeName, DiskSpaceData &data)
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
        spdlog::warn("GetDiskFreeSpaceExW failed for volume '{}' with error code: {}", volumeName, error);
        return false;
    }

    data.totalBytes = totalNumberOfBytes.QuadPart;
    data.freeBytes = freeBytesAvailable.QuadPart;
    return true;
}

} // namespace

domain::DiskSample calculateDiskSample(
    const std::string &volumeName,
    uint64_t totalBytes,
    uint64_t freeBytes)
{
    const uint64_t validFreeBytes = std::min(freeBytes, totalBytes);
    const uint64_t usedBytes = totalBytes - validFreeBytes;
    const float usagePercent = totalBytes > 0
        ? std::clamp(static_cast<float>(usedBytes) * 100.0f / static_cast<float>(totalBytes), 0.0f, 100.0f)
        : 0.0f;

    return domain::DiskSample{
        .volumeName = volumeName,
        .totalBytes = totalBytes,
        .freeBytes = validFreeBytes,
        .usagePercent = usagePercent,
    };
}

DiskCollector::DiskCollector()
    : m_enumerator(enumerateFixedDrives),
      m_reader(readDiskSpace)
{}

DiskCollector::DiskCollector(FixedDriveEnumerator enumerator, DiskSpaceReader reader)
    : m_enumerator(std::move(enumerator)),
      m_reader(std::move(reader))
{}

std::vector<domain::DiskSample> DiskCollector::collect()
{
    std::vector<domain::DiskSample> samples;
    if (!m_enumerator || !m_reader) {
        return samples;
    }

    const auto drives = m_enumerator();
    samples.reserve(drives.size());

    for (const auto &drive : drives) {
        DiskSpaceData data{};
        if (m_reader(drive, data)) {
            samples.push_back(calculateDiskSample(drive, data.totalBytes, data.freeBytes));
        }
    }

    return samples;
}

} // namespace sysmon::platform

