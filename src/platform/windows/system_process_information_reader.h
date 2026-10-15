#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

/**
 * The NTSTATUS values the process list reader handles, as plain integers so
 * unit tests can use them without Windows headers.
 */
namespace ntstatus
{
constexpr int32_t kSuccess = 0;
constexpr int32_t kInfoLengthMismatch = static_cast<int32_t>(0xC0000004);
constexpr int32_t kBufferTooSmall = static_cast<int32_t>(0xC0000023);
} // namespace ntstatus

/**
 * Calls NtQuerySystemInformation(SystemProcessInformation) once with buffer and
 * returns its NTSTATUS. Sets returnLength to the length Windows reported: the
 * bytes written on success, or the size needed when the buffer is too small.
 * Uses plain types so unit tests can fake it.
 */
using SystemProcessInformationQuery = std::function<int32_t(std::span<std::byte> buffer, uint32_t& returnLength)>;

/**
 * Returns the query backed by NtQuerySystemInformation, resolved from ntdll.dll,
 * or nullptr (after logging) if it cannot be resolved.
 */
[[nodiscard]] SystemProcessInformationQuery resolveSystemProcessInformationQuery();

/**
 * Reads the system process list with NtQuerySystemInformation(SystemProcessInformation),
 * which the default constructor resolves from ntdll.dll.
 *
 * Uses the resizing-buffer loop: when the buffer is too small, it grows to the
 * size Windows reported plus an eighth for processes and threads created before
 * the retry, or to twice its size if Windows reported no larger size, and the
 * query runs again, up to kMaxAttempts times in all. The buffer is kept
 * between reads, so once it has grown a read normally needs one call.
 *
 * Not thread-safe; each instance belongs to one caller.
 */
class SystemProcessInformationReader
{
public:
    static constexpr std::size_t kInitialBufferSize = 256 * 1024;
    // Far above any real process list (a list with 100,000 threads is about 10 MiB),
    // so a corrupt reported size cannot exhaust memory.
    static constexpr std::size_t kMaxBufferSize = 64 * 1024 * 1024;
    static constexpr int kMaxAttempts = 5;

    /** Uses NtQuerySystemInformation from ntdll.dll. */
    SystemProcessInformationReader();

    /** Uses the given query, e.g. a fake in unit tests. A null query makes every read fail. */
    explicit SystemProcessInformationReader(SystemProcessInformationQuery query,
                                            std::size_t initialBufferSize = kInitialBufferSize);

    /**
     * Returns the bytes the query wrote: the SYSTEM_PROCESS_INFORMATION entries,
     * each followed by its threads. The span is valid until the next read() or
     * until the reader is destroyed.
     *
     * Returns std::nullopt when NtQuerySystemInformation could not be resolved,
     * the query fails, the buffer is still too small after kMaxAttempts calls,
     * or the reported size exceeds kMaxBufferSize.
     */
    [[nodiscard]] std::optional<std::span<const std::byte>> read();

    /** Returns the current buffer size in bytes. */
    [[nodiscard]] std::size_t bufferSize() const;

private:
    SystemProcessInformationQuery m_query;
    std::vector<std::byte> m_buffer;
    RepeatedFailureLog m_failureLog{"NtQuerySystemInformation(SystemProcessInformation)"};
};

} // namespace sysmon::platform
