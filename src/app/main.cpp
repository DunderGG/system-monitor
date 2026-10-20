#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <spdlog/spdlog.h>

#include "app_logging.h"
#include "domain/settings.h"
#include "monitoring/sampling_scheduler.h"
#include "persistence/settings_file.h"
#include "platform/windows/current_process.h"
#include "platform/windows/process_terminator.h"
#include "settings_service.h"
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
int runMonitor(QApplication& application, const sysmon::domain::Settings& settings,
               const std::filesystem::path& settingsPath)
{
    sysmon::monitoring::SamplingScheduler scheduler;
    sysmon::app::registerSystemCollectors(scheduler);
    sysmon::app::applyRefreshInterval(scheduler, settings.refreshInterval);
    sysmon::app::SettingsService settingsService(settingsPath, settings, scheduler);

    sysmon::ui::MainWindow mainWindow;
    mainWindow.setSettings(settings);
    mainWindow.processesView()->setProcessTerminator(sysmon::platform::terminateOwnedProcess);
    if (const auto monitor = sysmon::platform::currentProcessIdentity()) {
        mainWindow.processesView()->setMonitorIdentity(*monitor);
    }

    QObject::connect(&scheduler, &sysmon::monitoring::SamplingScheduler::snapshotReady, &mainWindow,
                     &sysmon::ui::MainWindow::onSnapshotReady, Qt::QueuedConnection);
    // The window applies what its views show; the service applies the rest and saves the file (DC-127).
    QObject::connect(&mainWindow, &sysmon::ui::MainWindow::settingsChanged, &mainWindow,
                     [&settingsService, &mainWindow](const sysmon::domain::Settings& changed) {
                         if (const auto error = settingsService.change(changed)) {
                             mainWindow.showSettingsNotSaved(QString::fromStdWString(settingsService.path().wstring()),
                                                             QString::fromStdString(*error));
                         }
                     });

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
        "Set the log level for this run, in place of the one in the settings: trace, debug, info, warn, error, "
        "critical, or off.",
        "level",
    });
    commandLineParser.process(application);

    std::optional<sysmon::domain::LogLevel> commandLineLogLevel;
    if (commandLineParser.isSet("log-level")) {
        commandLineLogLevel = sysmon::app::parseLogLevel(commandLineParser.value("log-level").toStdString());
        if (!commandLineLogLevel) {
            commandLineParser.showHelp(EXIT_FAILURE);
        }
    }

    // The settings are read before the logger is configured, because they hold
    // its level; what reading them found is logged once it is (DC-127). They
    // roam with the user's profile, as Windows keeps settings, and the log
    // stays on this machine. The first save creates the settings' directory.
    const QString settingsDirectory =
        QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto settingsPath = std::filesystem::path{settingsDirectory.toStdWString()} / "settings.json";
    const auto loadedSettings = sysmon::persistence::loadSettingsFile(settingsPath);
    const sysmon::domain::Settings settings = loadedSettings ? loadedSettings->settings : sysmon::domain::Settings{};

    const QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDirectory);
    const auto logFilePath = std::filesystem::path{logDirectory.toStdWString()} / "system_monitor.log";
    const sysmon::domain::LogLevel logLevel = commandLineLogLevel.value_or(settings.logLevel);
    sysmon::app::configureLogging(logFilePath, sysmon::app::toSpdlogLevel(logLevel));
    spdlog::info("Application started with log level '{}' from {}", sysmon::app::logLevelName(logLevel),
                 commandLineLogLevel ? "the --log-level option" : "the settings");
    sysmon::app::logSettingsRead(settingsPath, loadedSettings);

    const int exitCode = runMonitor(application, settings, settingsPath);

    spdlog::info("Application exited with code {}", exitCode);
    spdlog::shutdown();
    return exitCode;
}
