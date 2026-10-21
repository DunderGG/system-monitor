#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>
#include <winternl.h>

#include <gtest/gtest.h>

#include "domain/process_info.h"
#include "platform/windows/system_process_information.h"

using namespace verdandi::domain;
using namespace verdandi::platform;

namespace
{

// 2024-01-01T00:00:00Z as a FILETIME value.
constexpr int64_t kNewYear2024FileTime = 133'485'408'000'000'000;

HANDLE pidHandle(uintptr_t pid)
{
    return reinterpret_cast<HANDLE>(pid);
}

// Lays out entries as NtQuerySystemInformation does: each entry, then its
// thread records, then its image name, with NextEntryOffset linking them.
class ProcessListBuilder
{
public:
    FullSystemProcessInformation& add(std::wstring imageName, ULONG threadCount = 0)
    {
        FullSystemProcessInformation entry{};
        entry.NumberOfThreads = threadCount;
        m_entries.push_back(Entry{.info = entry, .imageName = std::move(imageName)});
        return m_entries.back().info;
    }

    // Builds the buffer. The entries' ImageName pointers refer into it, so it
    // stays valid until the builder is destroyed or build() is called again.
    std::span<const std::byte> build()
    {
        std::vector<std::size_t> offsets;
        std::size_t size = 0;
        for (const auto& entry : m_entries) {
            offsets.push_back(size);
            size += entrySize(entry);
        }

        // uint64_t storage keeps the entries 8-byte aligned, as Windows does.
        m_storage.assign((size + sizeof(uint64_t) - 1) / sizeof(uint64_t), 0);
        auto* bytes = reinterpret_cast<std::byte*>(m_storage.data());

        for (std::size_t i = 0; i < m_entries.size(); ++i) {
            FullSystemProcessInformation info = m_entries[i].info;
            const std::wstring& name = m_entries[i].imageName;
            const std::size_t nameOffset =
                offsets[i] + sizeof(FullSystemProcessInformation) + info.NumberOfThreads * kThreadSize;

            info.NextEntryOffset = (i + 1 < m_entries.size()) ? static_cast<ULONG>(offsets[i + 1] - offsets[i]) : 0;
            if (!name.empty()) {
                std::memcpy(bytes + nameOffset, name.data(), name.size() * sizeof(WCHAR));
                info.ImageName.Length = static_cast<USHORT>(name.size() * sizeof(WCHAR));
                info.ImageName.MaximumLength = static_cast<USHORT>((name.size() + 1) * sizeof(WCHAR));
                info.ImageName.Buffer = reinterpret_cast<PWSTR>(bytes + nameOffset);
            }
            std::memcpy(bytes + offsets[i], &info, sizeof(info));
        }
        return {bytes, size};
    }

    // Returns a copy of a built entry, with its final NextEntryOffset and ImageName.
    FullSystemProcessInformation builtEntry(std::size_t index) const
    {
        FullSystemProcessInformation info{};
        std::memcpy(&info, builtBytes() + builtOffset(index), sizeof(info));
        return info;
    }

    // Overwrites a built entry in place, e.g. to corrupt the list.
    void patchBuiltEntry(std::size_t index, const FullSystemProcessInformation& info)
    {
        std::memcpy(reinterpret_cast<std::byte*>(m_storage.data()) + builtOffset(index), &info, sizeof(info));
    }

private:
    static constexpr std::size_t kThreadSize = sizeof(SYSTEM_THREAD_INFORMATION);

    struct Entry
    {
        FullSystemProcessInformation info;
        std::wstring imageName;
    };

    const std::byte* builtBytes() const
    {
        return reinterpret_cast<const std::byte*>(m_storage.data());
    }

    std::size_t builtOffset(std::size_t index) const
    {
        std::size_t offset = 0;
        for (std::size_t i = 0; i < index; ++i) {
            offset += entrySize(m_entries[i]);
        }
        return offset;
    }

    static std::size_t entrySize(const Entry& entry)
    {
        const std::size_t nameBytes = (entry.imageName.size() + 1) * sizeof(WCHAR);
        const std::size_t size =
            sizeof(FullSystemProcessInformation) + entry.info.NumberOfThreads * kThreadSize + nameBytes;
        return (size + 7) / 8 * 8;
    }

    std::vector<Entry> m_entries;
    std::vector<uint64_t> m_storage;
};

} // namespace

TEST(SystemProcessInformation, FileTimeToSystemClock_UnixEpoch_ReturnsSystemClockEpoch)
{
    EXPECT_EQ(fileTimeToSystemClock(116'444'736'000'000'000), std::chrono::system_clock::time_point{});
}

TEST(SystemProcessInformation, FileTimeToSystemClock_KnownDate_ConvertsExactly)
{
    using FileTimeTick = std::chrono::duration<int64_t, std::ratio<1, 10'000'000>>;
    const std::chrono::sys_days newYear2024 = std::chrono::year{2024} / std::chrono::January / 1;

    EXPECT_EQ(fileTimeToSystemClock(kNewYear2024FileTime), newYear2024);
    EXPECT_EQ(fileTimeToSystemClock(kNewYear2024FileTime + 1), newYear2024 + FileTimeTick{1});
}

TEST(SystemProcessInformation, Parse_SingleEntry_ExtractsEveryField)
{
    ProcessListBuilder builder;
    auto& entry = builder.add(L"app.exe", 3);
    entry.UniqueProcessId = pidHandle(1234);
    entry.InheritedFromUniqueProcessId = pidHandle(560);
    entry.CreateTime.QuadPart = kNewYear2024FileTime;
    entry.UserTime.QuadPart = 12'345'678; // 1234.5678 ms
    entry.KernelTime.QuadPart = 20'000'000;
    entry.HandleCount = 512;
    entry.WorkingSetSize = 64'000'000;
    entry.WorkingSetPrivateSize.QuadPart = 48'000'000;
    entry.PagefileUsage = 1;
    entry.PrivatePageCount = 32'000'000;
    entry.ReadOperationCount.QuadPart = 2;
    entry.ReadTransferCount.QuadPart = 1'000;
    entry.WriteOperationCount.QuadPart = 3;
    entry.WriteTransferCount.QuadPart = 500;
    entry.OtherTransferCount.QuadPart = 7;

    const auto processes = parseSystemProcessInformation(builder.build());

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 1u);
    const ProcessInfo& process = (*processes)[0];
    EXPECT_EQ(process.pid, 1234u);
    EXPECT_EQ(process.parentPid, 560u);
    EXPECT_EQ(process.creationTime, fileTimeToSystemClock(kNewYear2024FileTime));
    EXPECT_EQ(process.imageName, "app.exe");
    EXPECT_FALSE(process.imagePath.has_value());
    EXPECT_FALSE(process.commandLine.has_value());
    EXPECT_EQ(process.cpuUserTimeMs, 1234u);
    EXPECT_EQ(process.cpuKernelTimeMs, 2000u);
    EXPECT_EQ(process.workingSetBytes, 64'000'000u);
    EXPECT_EQ(process.privateWorkingSetBytes, 48'000'000u);
    EXPECT_EQ(process.privateBytes, 32'000'000u);
    EXPECT_EQ(process.ioReadBytes, 1'000u);
    EXPECT_EQ(process.ioWriteBytes, 500u);
    EXPECT_EQ(process.threadCount, 3u);
    EXPECT_EQ(process.handleCount, 512u);
}

TEST(SystemProcessInformation, Parse_NegativeSignedCounters_ReadAsZero)
{
    ProcessListBuilder builder;
    auto& entry = builder.add(L"app.exe");
    entry.WorkingSetPrivateSize.QuadPart = -4096;
    entry.UserTime.QuadPart = -1;
    entry.KernelTime.QuadPart = -1;
    entry.ReadTransferCount.QuadPart = -1;
    entry.WriteTransferCount.QuadPart = -1;

    const auto processes = parseSystemProcessInformation(builder.build());

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 1u);
    const ProcessInfo& process = (*processes)[0];
    EXPECT_EQ(process.privateWorkingSetBytes, 0u);
    EXPECT_EQ(process.cpuUserTimeMs, 0u);
    EXPECT_EQ(process.cpuKernelTimeMs, 0u);
    EXPECT_EQ(process.ioReadBytes, 0u);
    EXPECT_EQ(process.ioWriteBytes, 0u);
}

TEST(SystemProcessInformation, Parse_SeveralEntriesWithThreads_FollowsNextEntryOffsetInOrder)
{
    ProcessListBuilder builder;
    builder.add(L"", 8).UniqueProcessId = pidHandle(0);
    builder.add(L"System", 200).UniqueProcessId = pidHandle(4);
    builder.add(L"explorer.exe", 1).UniqueProcessId = pidHandle(7000);

    const auto processes = parseSystemProcessInformation(builder.build());

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 3u);
    EXPECT_EQ((*processes)[0].pid, 0u);
    EXPECT_EQ((*processes)[1].pid, 4u);
    EXPECT_EQ((*processes)[1].imageName, "System");
    EXPECT_EQ((*processes)[1].threadCount, 200u);
    EXPECT_EQ((*processes)[2].pid, 7000u);
    EXPECT_EQ((*processes)[2].imageName, "explorer.exe");
}

TEST(SystemProcessInformation, Parse_IdleProcess_HasEmptyName)
{
    ProcessListBuilder builder;
    builder.add(L"", 8);

    const auto processes = parseSystemProcessInformation(builder.build());

    ASSERT_TRUE(processes.has_value());
    ASSERT_EQ(processes->size(), 1u);
    EXPECT_EQ((*processes)[0].pid, 0u);
    EXPECT_TRUE((*processes)[0].imageName.empty());
}

TEST(SystemProcessInformation, Parse_NonAsciiName_ConvertsToUtf8)
{
    ProcessListBuilder builder;
    builder.add(L"Pr\x00F6gram.exe");

    const auto processes = parseSystemProcessInformation(builder.build());

    ASSERT_TRUE(processes.has_value());
    EXPECT_EQ((*processes)[0].imageName, "Pr\xC3\xB6gram.exe");
}

TEST(SystemProcessInformation, Parse_NameLengthShorterThanStoredText_UsesLength)
{
    ProcessListBuilder builder;
    builder.add(L"app.exe.extra");
    const auto bytes = builder.build();
    // Shorten the stored length to "app.exe", which is not null-terminated in the buffer.
    auto entry = builder.builtEntry(0);
    entry.ImageName.Length = static_cast<USHORT>(7 * sizeof(WCHAR));
    builder.patchBuiltEntry(0, entry);

    const auto processes = parseSystemProcessInformation(bytes);

    ASSERT_TRUE(processes.has_value());
    EXPECT_EQ((*processes)[0].imageName, "app.exe");
}

TEST(SystemProcessInformation, Parse_NameOutsideBuffer_LeftEmpty)
{
    std::wstring elsewhere = L"elsewhere.exe";
    ProcessListBuilder builder;
    builder.add(L"app.exe");
    const auto bytes = builder.build();
    auto entry = builder.builtEntry(0);
    entry.ImageName.Buffer = elsewhere.data();
    builder.patchBuiltEntry(0, entry);

    const auto processes = parseSystemProcessInformation(bytes);

    ASSERT_TRUE(processes.has_value());
    EXPECT_TRUE((*processes)[0].imageName.empty());
}

TEST(SystemProcessInformation, Parse_NextEntryBeyondBuffer_ReturnsNullopt)
{
    ProcessListBuilder builder;
    builder.add(L"a.exe");
    builder.add(L"b.exe");
    const auto bytes = builder.build();
    auto entry = builder.builtEntry(0);
    entry.NextEntryOffset = static_cast<ULONG>(bytes.size());
    builder.patchBuiltEntry(0, entry);

    EXPECT_FALSE(parseSystemProcessInformation(bytes).has_value());
}

TEST(SystemProcessInformation, Parse_LastEntryTruncated_ReturnsNullopt)
{
    ProcessListBuilder builder;
    builder.add(L"a.exe");
    builder.add(L"b.exe");
    const auto bytes = builder.build();

    // Cut the buffer in the middle of the second entry.
    const auto truncated =
        bytes.first(builder.builtEntry(0).NextEntryOffset + sizeof(FullSystemProcessInformation) / 2);

    EXPECT_FALSE(parseSystemProcessInformation(truncated).has_value());
}

TEST(SystemProcessInformation, Parse_EmptyBuffer_ReturnsNullopt)
{
    EXPECT_FALSE(parseSystemProcessInformation({}).has_value());
}

TEST(SystemProcessInformation, Parse_UnalignedBuffer_StillParses)
{
    ProcessListBuilder builder;
    builder.add(L"app.exe").UniqueProcessId = pidHandle(42);
    const auto bytes = builder.build();
    std::vector<std::byte> shifted(bytes.size() + 1);
    std::memcpy(shifted.data() + 1, bytes.data(), bytes.size());

    // The name pointer still refers to the original buffer, so only the entry is checked.
    const auto processes = parseSystemProcessInformation(std::span<const std::byte>(shifted).subspan(1));

    ASSERT_TRUE(processes.has_value());
    EXPECT_EQ((*processes)[0].pid, 42u);
}
