#include <cstdlib>
#include <filesystem>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <spdlog/spdlog.h>

#include "app_logging.h"
#include "monitoring/sampling_scheduler.h"
#include "platform/windows/current_process.h"
#include "platform/windows/process_terminator.h"
#include "system_collectors.h"
#include "ui/main_window.h"
#include "ui/processes_view.h"

namespace
{

// Runs the monitor until its window closes and returns the exit code. The
// scheduler, its collectors and the window are local to this function, so
// they are destroyed when it returns, before main() shuts the logger down:
// their teardown can log, and spdlog's free functions dereference the default
// logger without a check once spdlog::shutdown() has dropped it.
int runMonitor(QApplication& application)
{
    sysmon::monitoring::SamplingScheduler scheduler;
    sysmon::app::registerSystemCollectors(scheduler);

    sysmon::ui::MainWindow mainWindow;
    mainWindow.processesView()->setProcessTerminator(sysmon::platform::terminateOwnedProcess);
    if (const auto monitor = sysmon::platform::currentProcessIdentity()) {
        mainWindow.processesView()->setMonitorIdentity(*monitor);
    }

    QObject::connect(&scheduler, &sysmon::monitoring::SamplingScheduler::snapshotReady, &mainWindow,
                     &sysmon::ui::MainWindow::onSnapshotReady, Qt::QueuedConnection);

    scheduler.start();
    mainWindow.show();

    const int exitCode = application.exec();

    scheduler.stop();
    return exitCode;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QCoreApplication::setApplicationName("System Monitor");
    QCoreApplication::setOrganizationName("System Monitor");

    QCommandLineParser commandLineParser;
    commandLineParser.setApplicationDescription("A lightweight Windows desktop system monitor");
    commandLineParser.addHelpOption();
    commandLineParser.addOption({
        {"l", "log-level"},
        "Set the development log level: trace, debug, info, warn, error, critical, or off.",
        "level",
        "info",
    });
    commandLineParser.process(application);

    const std::string logLevelValue = commandLineParser.value("log-level").toStdString();
    const auto logLevel = sysmon::app::parseLogLevel(logLevelValue);
    if (!logLevel.has_value()) {
        commandLineParser.showHelp(EXIT_FAILURE);
    }

    const QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDirectory);
    const auto logFilePath = std::filesystem::path{logDirectory.toStdWString()} / "system_monitor.log";
    sysmon::app::configureLogging(logFilePath, *logLevel);
    spdlog::info("Application started with log level '{}'", logLevelValue);

    const int exitCode = runMonitor(application);

    spdlog::info("Application exited with code {}", exitCode);
    spdlog::shutdown();
    return exitCode;
}
