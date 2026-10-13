#include "platform/windows/process_collector.h"

#include <optional>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "platform/windows/repeated_failure_log.h"
#include "platform/windows/system_process_information.h"
#include "platform/windows/system_process_information_reader.h"

namespace sysmon::platform
{

ProcessCollector::ProcessCollector()
    : ProcessCollector([reader = SystemProcessInformationReader{},
                        parseFailureLog = RepeatedFailureLog{
                            "Parsing the process list"}]() mutable -> std::optional<std::vector<domain::ProcessInfo>> {
          const auto buffer = reader.read();
          if (!buffer) {
              return std::nullopt;
          }

          auto processes = parseSystemProcessInformation(*buffer);
          if (!processes) {
              parseFailureLog.failure(spdlog::level::err,
                                      "NtQuerySystemInformation(SystemProcessInformation) returned a malformed "
                                      "process list ({} bytes)",
                                      buffer->size());
              return std::nullopt;
          }
          parseFailureLog.success();
          return processes;
      })
{}

ProcessCollector::ProcessCollector(ProcessListReader listReader) : m_listReader(std::move(listReader)) {}

std::optional<std::vector<domain::ProcessInfo>> ProcessCollector::collect()
{
    if (!m_listReader) {
        return std::nullopt;
    }
    return m_listReader();
}

} // namespace sysmon::platform
