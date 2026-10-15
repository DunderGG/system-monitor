#include "platform/windows/system_process_information.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <windows.h>
#include <winternl.h>

#include "platform/windows/utf8_conversion.h"

namespace sysmon::platform
{

namespace
{

// 100 ns intervals from 1601-01-01 (the FILETIME epoch) to 1970-01-01.
constexpr int64_t kFileTimeToUnixEpoch = 116'444'736'000'000'000;
constexpr int64_t kFileTimeTicksPerMillisecond = 10'000;

using FileTimeTicks = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;

uint64_t nonNegative(const LARGE_INTEGER& value)
{
    return value.QuadPart > 0 ? static_cast<uint64_t>(value.QuadPart) : 0;
}

uint32_t handleToPid(HANDLE handle)
{
    return static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(handle));
}

// Returns the image name if its characters lie inside the buffer, else an empty
// string. ImageName is not necessarily null-terminated, so Length is used.
std::string imageNameOf(const UNICODE_STRING& name, std::span<const std::byte> buffer)
{
    if (name.Buffer == nullptr || name.Length == 0) {
        return {};
    }

    const auto bufferStart = reinterpret_cast<std::uintptr_t>(buffer.data());
    const auto nameStart = reinterpret_cast<std::uintptr_t>(name.Buffer);
    if (nameStart < bufferStart || nameStart - bufferStart > buffer.size() ||
        name.Length > buffer.size() - (nameStart - bufferStart)) {
        return {};
    }

    std::wstring characters(name.Length / sizeof(WCHAR), L'\0');
    std::memcpy(characters.data(), buffer.data() + (nameStart - bufferStart), characters.size() * sizeof(WCHAR));
    return wideToUtf8(characters);
}

domain::ProcessInfo toProcessInfo(const FullSystemProcessInformation& entry, std::span<const std::byte> buffer)
{
    return domain::ProcessInfo{
        .pid = handleToPid(entry.UniqueProcessId),
        .creationTime = fileTimeToSystemClock(entry.CreateTime.QuadPart),
        .parentPid = handleToPid(entry.InheritedFromUniqueProcessId),
        .imageName = imageNameOf(entry.ImageName, buffer),
        .cpuUserTimeMs = nonNegative(entry.UserTime) / kFileTimeTicksPerMillisecond,
        .cpuKernelTimeMs = nonNegative(entry.KernelTime) / kFileTimeTicksPerMillisecond,
        .workingSetBytes = entry.WorkingSetSize,
        .privateWorkingSetBytes = nonNegative(entry.WorkingSetPrivateSize),
        .privateBytes = entry.PrivatePageCount,
        .ioReadBytes = nonNegative(entry.ReadTransferCount),
        .ioWriteBytes = nonNegative(entry.WriteTransferCount),
        .threadCount = entry.NumberOfThreads,
        .handleCount = entry.HandleCount,
    };
}

} // namespace

std::chrono::system_clock::time_point fileTimeToSystemClock(int64_t fileTime)
{
    return std::chrono::system_clock::time_point{std::chrono::duration_cast<std::chrono::system_clock::duration>(
        FileTimeTicks{fileTime - kFileTimeToUnixEpoch})};
}

std::optional<std::vector<domain::ProcessInfo>> parseSystemProcessInformation(std::span<const std::byte> buffer)
{
    std::vector<domain::ProcessInfo> processes;
    std::size_t offset = 0;

    while (true) {
        if (offset > buffer.size() || buffer.size() - offset < sizeof(FullSystemProcessInformation)) {
            return std::nullopt;
        }

        FullSystemProcessInformation entry{};
        std::memcpy(&entry, buffer.data() + offset, sizeof(entry));
        processes.push_back(toProcessInfo(entry, buffer));

        if (entry.NextEntryOffset == 0) {
            return processes;
        }
        offset += entry.NextEntryOffset;
    }
}

} // namespace sysmon::platform
