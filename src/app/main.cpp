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
#include "power_event_filter.h"
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
int runMonitor(QApplication& application, const verdandi::domain::Settings& settings,
               const std::filesystem::path& settingsPath)
{
    verdandi::monitoring::SamplingScheduler scheduler;
    verdandi::app::registerSystemCollectors(scheduler);
    verdandi::app::applyRefreshInterval(scheduler, settings.refreshInterval);
    verdandi::app::SettingsService settingsService(settingsPath, settings, scheduler);
    // Sampling pauses while the system sleeps, and no rate spans the sleep (DC-129).
    verdandi::app::PowerEventFilter powerEventFilter(scheduler);
    application.installNativeEventFilter(&powerEventFilter);

    verdandi::ui::MainWindow mainWindow;
    mainWindow.setSettings(settings);
    mainWindow.processesView()->setProcessTerminator(verdandi::platform::terminateOwnedProcess);
    if (const auto monitor = verdandi::platform::currentProcessIdentity()) {
        mainWindow.processesView()->setMonitorIdentity(*monitor);
    }

    QObject::connect(&scheduler, &verdandi::monitoring::SamplingScheduler::snapshotReady, &mainWindow,
                     &verdandi::ui::MainWindow::onSnapshotReady, Qt::QueuedConnection);
    // The window applies what its views show; the service applies the rest and saves the file (DC-127).
    QObject::connect(&mainWindow, &verdandi::ui::MainWindow::settingsChanged, &mainWindow,
                     [&settingsService, &mainWindow](const verdandi::domain::Settings& changed) {
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

    // With no organization name, Qt's folders for the settings and the log are
    // %APPDATA%\Verdandi and %LOCALAPPDATA%\Verdandi (DC-131).
    QCoreApplication::setApplicationName("Verdandi");

    // A GUI program has no console, so Qt shows the help and these errors in a
    // message box, unless standard output is redirected (DC-134).
    const QString logLevelNames = "trace, debug, info, warn, error, critical, or off";
    QCommandLineParser commandLineParser;
    commandLineParser.setApplicationDescription("A lightweight Windows desktop system monitor");
    commandLineParser.addHelpOption();
    commandLineParser.addOption({
        {"l", "log-level"},
        QString("Set the log level for this run, in place of the one in the settings: %1.").arg(logLevelNames),
        "level",
    });
    commandLineParser.process(application);

    std::optional<verdandi::domain::LogLevel> commandLineLogLevel;
    if (commandLineParser.isSet("log-level")) {
        const QString logLevelName = commandLineParser.value("log-level");
        commandLineLogLevel = verdandi::app::parseLogLevel(logLevelName.toStdString());
        if (!commandLineLogLevel) {
            // Worded as Qt words an unknown option.
            QCommandLineParser::showMessageAndExit(
                QCommandLineParser::MessageType::Error,
                QString("%1: Unknown log level '%2'. Use %3.")
                    .arg(QCoreApplication::applicationName(), logLevelName, logLevelNames),
                EXIT_FAILURE);
        }
    }

    // The settings are read before the logger is configured, because they hold
    // its level; what reading them found is logged once it is (DC-127). They
    // roam with the user's profile, as Windows keeps settings, and the log
    // stays on this machine. The first save creates the settings' directory.
    const QString settingsDirectory =
        QDir::toNativeSeparators(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const auto settingsPath = std::filesystem::path{settingsDirectory.toStdWString()} / "settings.json";
    const auto loadedSettings = verdandi::persistence::loadSettingsFile(settingsPath);
    const verdandi::domain::Settings settings =
        loadedSettings ? loadedSettings->settings : verdandi::domain::Settings{};

    const QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(logDirectory);
    const auto logFilePath = std::filesystem::path{logDirectory.toStdWString()} / "verdandi.log";
    const verdandi::domain::LogLevel logLevel = commandLineLogLevel.value_or(settings.logLevel);
    verdandi::app::configureLogging(logFilePath, verdandi::app::toSpdlogLevel(logLevel));
    spdlog::info("Application started with log level '{}' from {}", verdandi::app::logLevelName(logLevel),
                 commandLineLogLevel ? "the --log-level option" : "the settings");
    verdandi::app::logSettingsRead(settingsPath, loadedSettings);

    const int exitCode = runMonitor(application, settings, settingsPath);

    spdlog::info("Application exited with code {}", exitCode);
    spdlog::shutdown();
    return exitCode;
}
