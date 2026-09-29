#include "platform/windows/performance_info_reader.h"

#include <windows.h>
#include <psapi.h>

#include <spdlog/spdlog.h>

#include "platform/windows/repeated_failure_log.h"

namespace sysmon::platform
{

PerformanceInfoReader makePerformanceInfoReader()
{
    return [failureLog = RepeatedFailureLog{"GetPerformanceInfo"}](PerformanceInfoData& data) mutable {
        PERFORMANCE_INFORMATION info{};
        if (!::GetPerformanceInfo(&info, sizeof(info))) {
            const DWORD error = ::GetLastError();
            failureLog.failure(spdlog::level::warn, "GetPerformanceInfo failed with error code: {}", error);
            return false;
        }
        failureLog.success();
        data.pageSize = info.PageSize;
        data.systemCachePages = info.SystemCache;
        data.kernelPagedPages = info.KernelPaged;
        data.kernelNonPagedPages = info.KernelNonpaged;
        data.processCount = info.ProcessCount;
        data.threadCount = info.ThreadCount;
        data.handleCount = info.HandleCount;
        return true;
    };
}

} // namespace sysmon::platform
