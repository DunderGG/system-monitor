#pragma once

#include <thread>

#include <QSysInfo>
#include <QtGlobal>
#include <spdlog/spdlog.h>

namespace verdandi::benchmarks
{

// Logs what a benchmark's timings depend on besides the code: the compiler
// and configuration it was built with, the Qt version it runs with, Windows,
// and the number of logical processors. build.ps1 prints the processor and the
// power settings before it runs the benchmarks. Recorded with a table of
// figures, they tell a change in the environment apart from a regression.
inline void logBenchmarkEnvironment()
{
#ifdef NDEBUG
    constexpr const char* kConfiguration = "Release";
#else
    constexpr const char* kConfiguration = "Debug";
#endif
    spdlog::info("Built with MSVC {}.{:02}.{} ({}), running Qt {} on {} ({}), {} logical processors.\n", _MSC_VER / 100,
                 _MSC_VER % 100, _MSC_FULL_VER % 100000, kConfiguration, qVersion(),
                 QSysInfo::prettyProductName().toStdString(), QSysInfo::kernelVersion().toStdString(),
                 std::thread::hardware_concurrency());
}

} // namespace verdandi::benchmarks
