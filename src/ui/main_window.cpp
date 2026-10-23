#include "ui/main_window.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QTabWidget>
#include <QUrl>

#include "ui/dashboard_view.h"
#include "ui/network_view.h"
#include "ui/performance_view.h"
#include "ui/process_model.h"
#include "ui/processes_view.h"
#include "ui/settings_dialog.h"
#include "verdandi_version.h"

namespace verdandi::ui
{

namespace
{

// Wide enough for several readout columns on the Performance pages. The
// window still fits its content at 800x600.
constexpr QSize kDefaultSize{1024, 720};
// The largest share of the screen's available area the window opens at.
constexpr double kMaxScreenShare = 0.9;

// Opens a window-modal warning over parent that says text, then the path of
// the file concerned and the system's reason, and then what follows from it.
void openFileWarning(QWidget* parent, const QString& title, const QString& text, const QString& path,
                     const QString& reason, const QString& consequence)
{
    auto* box = new QMessageBox(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    // The path and the reason come from the system; neither is rich text.
    box->setTextFormat(Qt::PlainText);
    box->setInformativeText(QString("%1\n%2\n\n%3").arg(path, reason, consequence));
    box->open();
}

// Returns a rich-text link to the file named fileName in the executable's folder.
QString linkToFileBesideExecutable(const QString& fileName)
{
    const QUrl url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/" + fileName);
    return QString("<a href=\"%1\">%2</a>").arg(url.toString().toHtmlEscaped(), fileName);
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("Verdandi");
    QSize size = kDefaultSize;
    if (const QScreen* current = screen()) {
        size = size.boundedTo(current->availableGeometry().size() * kMaxScreenShare);
    }
    resize(size);

    m_tabWidget = new QTabWidget(this);

    m_dashboardView = new DashboardView(this);
    m_performanceView = new PerformanceView(this);
    m_processesView = new ProcessesView(this);
    m_networkView = new NetworkView(this);

    m_tabWidget->addTab(m_dashboardView, "Dashboard");
    m_tabWidget->addTab(m_performanceView, "Performance");
    m_tabWidget->addTab(m_processesView, "Processes");
    m_tabWidget->addTab(m_networkView, "Network");

    setCentralWidget(m_tabWidget);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    m_settingsAction = fileMenu->addAction("&Settings...");
    fileMenu->addSeparator();
    m_exitAction = fileMenu->addAction("E&xit");
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* helpMenu = menuBar()->addMenu("&Help");
    m_aboutAction = helpMenu->addAction("&About Verdandi");
    QAction* aboutQtAction = helpMenu->addAction("About &Qt");
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::openAboutBox);
    connect(aboutQtAction, &QAction::triggered, this, &QApplication::aboutQt);
}

QTabWidget* MainWindow::tabWidget() const
{
    return m_tabWidget;
}

DashboardView* MainWindow::dashboardView() const
{
    return m_dashboardView;
}

PerformanceView* MainWindow::performanceView() const
{
    return m_performanceView;
}

ProcessesView* MainWindow::processesView() const
{
    return m_processesView;
}

NetworkView* MainWindow::networkView() const
{
    return m_networkView;
}

QAction* MainWindow::settingsAction() const
{
    return m_settingsAction;
}

QAction* MainWindow::exitAction() const
{
    return m_exitAction;
}

QAction* MainWindow::aboutAction() const
{
    return m_aboutAction;
}

void MainWindow::setSettings(const domain::Settings& settings)
{
    m_settings = settings;
    m_dashboardView->setPanels(settings.dashboardPanels);
    m_dashboardView->setDisplayUnits(settings.displayUnits);
    m_performanceView->setSampleInterval(settings.refreshInterval);
    m_performanceView->setDisplayUnits(settings.displayUnits);
    m_processesView->model()->setByteUnits(settings.displayUnits.bytes);
    m_networkView->setSampleInterval(settings.refreshInterval);
    m_networkView->setDisplayUnits(settings.displayUnits);
}

const domain::Settings& MainWindow::settings() const
{
    return m_settings;
}

void MainWindow::showSettingsNotSaved(const QString& path, const QString& reason)
{
    openFileWarning(this, "Settings", "The settings could not be saved.", path, reason,
                    "The changed settings apply until Verdandi closes.");
}

void MainWindow::showLogNotWritten(const QString& path, const QString& reason)
{
    openFileWarning(this, "Log", "The log cannot be written.", path, reason,
                    "Verdandi runs without its log file until it starts again.");
}

void MainWindow::openSettingsDialog()
{
    auto* dialog = new SettingsDialog(m_settings, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        const domain::Settings edited = dialog->settings();
        if (edited != m_settings) {
            setSettings(edited);
            emit settingsChanged(m_settings);
        }
    });
    dialog->open();
}

void MainWindow::openAboutBox()
{
    // Window-modal, as the other message boxes are; QMessageBox::about() is
    // application-modal and blocks. The text is the program's own, so it is
    // rich text, with links.
    auto* box = new QMessageBox(QMessageBox::Information, "About Verdandi", QString(), QMessageBox::Ok, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setTextFormat(Qt::RichText);
    box->setText(QString("<b>Verdandi %1</b><br>%2")
                     .arg(QCoreApplication::applicationVersion().toHtmlEscaped(), VERDANDI_DESCRIPTION));
    const QUrl homepage(VERDANDI_HOMEPAGE_URL);
    box->setInformativeText(
        QString("<p>%1<br><a href=\"%2\">%3</a></p>"
                "<p>Verdandi is free software under the GNU General Public License, version 3 (%4), and comes with "
                "no warranty.</p>"
                "<p>It uses Qt %5 and other open-source libraries, whose licenses are in %6.</p>")
            .arg(VERDANDI_COPYRIGHT, homepage.toString(), homepage.host() + homepage.path(),
                 linkToFileBesideExecutable("LICENSE.txt"), qVersion(),
                 linkToFileBesideExecutable("THIRD_PARTY_NOTICES.txt")));
    box->open();
}

void MainWindow::onSnapshotReady(const verdandi::domain::SystemSnapshot& snapshot)
{
    // Every view records every snapshot so its history has no holes when it is
    // shown; each view skips repainting its own widgets while it is hidden.
    if (m_dashboardView != nullptr) {
        m_dashboardView->updateSnapshot(snapshot);
    }
    if (m_performanceView != nullptr) {
        m_performanceView->updateSnapshot(snapshot);
    }
    if (m_processesView != nullptr) {
        m_processesView->updateSnapshot(snapshot);
    }
    if (m_networkView != nullptr) {
        m_networkView->updateSnapshot(snapshot);
    }
}

} // namespace verdandi::ui
