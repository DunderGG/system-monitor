#include "platform/windows/process_identity_cache.h"

#include <cstddef>
#include <unordered_map>
#include <utility>
#include <vector>

namespace verdandi::platform
{

ProcessIdentityCache::ProcessIdentityCache(ProcessDetailsReader reader) : m_reader(std::move(reader)) {}

void ProcessIdentityCache::apply(std::vector<domain::ProcessInfo>& processes)
{
    ++m_generation;

    for (auto& process : processes) {
        auto [it, isNew] = m_entries.try_emplace(process.identity());
        Entry& entry = it->second;
        if (isNew && m_reader) {
            entry.details = m_reader(process);
        }
        entry.lastSeen = m_generation;

        process.imagePath = entry.details.imagePath;
        process.commandLine = entry.details.commandLine;
        process.detailsStatus = entry.details.status;
    }

    std::erase_if(m_entries, [this](const auto& item) { return item.second.lastSeen != m_generation; });
}

std::size_t ProcessIdentityCache::size() const
{
    return m_entries.size();
}

} // namespace verdandi::platform
