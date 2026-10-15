#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <windows.h>
#include <winternl.h>

#include "domain/process_info.h"

namespace sysmon::platform
{

/**
 * The complete layout of one SYSTEM_PROCESS_INFORMATION entry, as written by
 * NtQuerySystemInformation(SystemProcessInformation) and followed in the buffer
 * by NumberOfThreads SYSTEM_THREAD_INFORMATION records.
 *
 * winternl.h declares the same struct but hides the creation time, CPU times,
 * parent PID, and I/O counters in Reserved fields; this definition names them.
 * Field names follow the Windows definition, and the static_asserts below tie
 * the layout to winternl.h.
 *
 * Internal header: it exposes Windows types so tests/platform/ can build
 * process lists. Other modules must not include it.
 */
struct FullSystemProcessInformation
{
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
};

static_assert(sizeof(FullSystemProcessInformation) == sizeof(SYSTEM_PROCESS_INFORMATION));
static_assert(offsetof(FullSystemProcessInformation, WorkingSetPrivateSize) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, Reserved1));
static_assert(offsetof(FullSystemProcessInformation, ImageName) == offsetof(SYSTEM_PROCESS_INFORMATION, ImageName));
static_assert(offsetof(FullSystemProcessInformation, UniqueProcessId) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, UniqueProcessId));
static_assert(offsetof(FullSystemProcessInformation, InheritedFromUniqueProcessId) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, Reserved2));
static_assert(offsetof(FullSystemProcessInformation, HandleCount) == offsetof(SYSTEM_PROCESS_INFORMATION, HandleCount));
static_assert(offsetof(FullSystemProcessInformation, WorkingSetSize) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, WorkingSetSize));
static_assert(offsetof(FullSystemProcessInformation, PrivatePageCount) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, PrivatePageCount));
static_assert(offsetof(FullSystemProcessInformation, ReadOperationCount) ==
              offsetof(SYSTEM_PROCESS_INFORMATION, Reserved7));

/**
 * Converts a FILETIME value (100 ns intervals since 1601-01-01 UTC) to
 * system_clock, which counts from 1970-01-01 UTC.
 */
[[nodiscard]] std::chrono::system_clock::time_point fileTimeToSystemClock(int64_t fileTime);

/**
 * Converts the list that NtQuerySystemInformation(SystemProcessInformation)
 * wrote into ProcessInfo values, following NextEntryOffset from the first entry
 * at the start of the buffer.
 *
 * Fills every field the list provides. privateWorkingSetBytes is
 * WorkingSetPrivateSize, the working set without shared pages (Task Manager's
 * "Memory"). privateBytes is PrivatePageCount, the commit charge (Task
 * Manager's "Commit size"). ioReadBytes and ioWriteBytes are the read and
 * write transfer counts. CPU times are truncated to whole milliseconds. A
 * negative value in a signed (LARGE_INTEGER) counter becomes zero. The Idle
 * process (PID 0) has no image name, so its imageName is empty. imagePath and
 * commandLine stay std::nullopt; reading them requires opening the process.
 *
 * Each entry is copied out of the buffer, so the buffer needs no particular
 * alignment. Returns std::nullopt if an entry does not fit in the buffer. An
 * image name that does not lie inside the buffer is left empty.
 */
[[nodiscard]] std::optional<std::vector<domain::ProcessInfo>>
parseSystemProcessInformation(std::span<const std::byte> buffer);

} // namespace sysmon::platform
