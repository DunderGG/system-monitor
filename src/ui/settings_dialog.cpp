#include "ui/settings_dialog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLayoutItem>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include "ui/view_metrics.h"

namespace verdandi::ui
{

namespace
{

// A combo box item: the text it shows and the enumerator it stands for.
template <typename Enum> struct ComboItem
{
    const char* text{};
    Enum value{};
};

constexpr std::array kLogLevelItems{
    ComboItem<domain::LogLevel>{"Trace", domain::LogLevel::Trace},
    ComboItem<domain::LogLevel>{"Debug", domain::LogLevel::Debug},
    ComboItem<domain::LogLevel>{"Info", domain::LogLevel::Info},
    ComboItem<domain::LogLevel>{"Warning", domain::LogLevel::Warn},
    ComboItem<domain::LogLevel>{"Error", domain::LogLevel::Error},
    ComboItem<domain::LogLevel>{"Critical", domain::LogLevel::Critical},
    ComboItem<domain::LogLevel>{"Off", domain::LogLevel::Off},
};

constexpr std::array kByteUnitsItems{
    ComboItem<domain::ByteUnits>{"Binary (KiB, MiB, GiB)", domain::ByteUnits::Binary},
    ComboItem<domain::ByteUnits>{"Decimal (KB, MB, GB)", domain::ByteUnits::Decimal},
};

constexpr std::array kNetworkRateUnitsItems{
    ComboItem<domain::NetworkRateUnits>{"Bytes per second", domain::NetworkRateUnits::BytesPerSecond},
    ComboItem<domain::NetworkRateUnits>{"Bits per second (Kbps, Mbps, Gbps)", domain::NetworkRateUnits::BitsPerSecond},
};

constexpr double kMillisecondsPerSecond = 1000.0;
// One decimal shows every half-second step, which reaches each of Process
// Explorer's update speeds.
constexpr int kRefreshIntervalDecimals = 1;

double toSeconds(std::chrono::milliseconds interval)
{
    return static_cast<double>(interval.count()) / kMillisecondsPerSecond;
}

template <typename Enum, std::size_t Count>
QComboBox* createComboBox(const std::array<ComboItem<Enum>, Count>& items, QWidget* parent)
{
    auto* comboBox = new QComboBox(parent);
    for (const auto& item : items) {
        comboBox->addItem(item.text, static_cast<int>(item.value));
    }
    return comboBox;
}

template <typename Enum> void selectValue(QComboBox* comboBox, Enum value)
{
    comboBox->setCurrentIndex(comboBox->findData(static_cast<int>(value)));
}

template <typename Enum> Enum selectedValue(const QComboBox* comboBox)
{
    return static_cast<Enum>(comboBox->currentData().toInt());
}

// Widens the label column of each form to the widest of them, so that the
// fields of one group box line up with those of the next.
void alignLabelColumns(std::initializer_list<const QFormLayout*> forms)
{
    std::vector<QWidget*> labels;
    for (const QFormLayout* form : forms) {
        for (int row = 0; row < form->rowCount(); ++row) {
            const QLayoutItem* item = form->itemAt(row, QFormLayout::LabelRole);
            if (item != nullptr && item->widget() != nullptr) {
                labels.push_back(item->widget());
            }
        }
    }

    int width = 0;
    for (const QWidget* label : labels) {
        width = std::max(width, label->sizeHint().width());
    }
    for (QWidget* label : labels) {
        label->setMinimumWidth(width);
    }
}

} // namespace

SettingsDialog::SettingsDialog(const domain::Settings& settings, QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Settings");

    m_refreshIntervalSpinBox = new QDoubleSpinBox(this);
    // A decimal point whatever the system's locale, as everywhere else in the UI (DC-57).
    m_refreshIntervalSpinBox->setLocale(QLocale::c());
    m_refreshIntervalSpinBox->setDecimals(kRefreshIntervalDecimals);
    m_refreshIntervalSpinBox->setRange(toSeconds(domain::kMinRefreshInterval), toSeconds(domain::kMaxRefreshInterval));
    m_refreshIntervalSpinBox->setSingleStep(toSeconds(domain::kRefreshIntervalStep));
    m_refreshIntervalSpinBox->setSuffix(" s");
    m_refreshIntervalSpinBox->setToolTip(
        "How often CPU, memory, disk and network use are read and shown, in steps of half a second");
    // A value typed between two steps shows the step that OK would save.
    connect(m_refreshIntervalSpinBox, &QDoubleSpinBox::editingFinished, this,
            [this] { m_refreshIntervalSpinBox->setValue(toSeconds(shownRefreshInterval())); });
    m_logLevelComboBox = createComboBox(kLogLevelItems, this);
    m_logLevelComboBox->setToolTip("The least severe messages that the log file keeps");

    auto* generalGroup = new QGroupBox("General", this);
    auto* generalLayout = new QFormLayout(generalGroup);
    generalLayout->addRow("&Refresh interval:", m_refreshIntervalSpinBox);
    generalLayout->addRow("&Log level:", m_logLevelComboBox);

    m_byteUnitsComboBox = createComboBox(kByteUnitsItems, this);
    m_byteUnitsComboBox->setToolTip("The units of memory, disk space, and I/O and network rates in bytes");
    m_networkRateUnitsComboBox = createComboBox(kNetworkRateUnitsItems, this);
    m_networkRateUnitsComboBox->setToolTip("Bits per second are decimal, as link speeds are");

    auto* unitsGroup = new QGroupBox("Units", this);
    auto* unitsLayout = new QFormLayout(unitsGroup);
    unitsLayout->addRow("&Byte units:", m_byteUnitsComboBox);
    unitsLayout->addRow("&Network throughput:", m_networkRateUnitsComboBox);
    alignLabelColumns({generalLayout, unitsLayout});

    // Named as the dashboard's cards are.
    m_cpuPanelCheckBox = new QCheckBox("&CPU", this);
    m_memoryPanelCheckBox = new QCheckBox("&Memory", this);
    m_diskPanelCheckBox = new QCheckBox("&Disk", this);
    m_networkPanelCheckBox = new QCheckBox("N&etwork", this);
    m_uptimePanelCheckBox = new QCheckBox("&Uptime", this);

    auto* dashboardGroup = new QGroupBox("Dashboard cards", this);
    auto* dashboardLayout = new QVBoxLayout(dashboardGroup);
    dashboardLayout->addWidget(m_cpuPanelCheckBox);
    dashboardLayout->addWidget(m_memoryPanelCheckBox);
    dashboardLayout->addWidget(m_diskPanelCheckBox);
    dashboardLayout->addWidget(m_networkPanelCheckBox);
    dashboardLayout->addWidget(m_uptimePanelCheckBox);

    m_buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] { setSettings(domain::Settings{}); });

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(kSectionSpacing);
    layout->addWidget(generalGroup);
    layout->addWidget(unitsGroup);
    layout->addWidget(dashboardGroup);
    layout->addWidget(m_buttonBox);
    // Nothing in the dialog gains from more room, so it keeps its size.
    layout->setSizeConstraint(QLayout::SetFixedSize);

    setSettings(settings);
}

void SettingsDialog::setSettings(const domain::Settings& settings)
{
    m_refreshIntervalSpinBox->setValue(toSeconds(domain::nearestRefreshInterval(settings.refreshInterval)));

    selectValue(m_logLevelComboBox, settings.logLevel);
    selectValue(m_byteUnitsComboBox, settings.displayUnits.bytes);
    selectValue(m_networkRateUnitsComboBox, settings.displayUnits.networkRates);

    m_cpuPanelCheckBox->setChecked(settings.dashboardPanels.cpu);
    m_memoryPanelCheckBox->setChecked(settings.dashboardPanels.memory);
    m_diskPanelCheckBox->setChecked(settings.dashboardPanels.disk);
    m_networkPanelCheckBox->setChecked(settings.dashboardPanels.network);
    m_uptimePanelCheckBox->setChecked(settings.dashboardPanels.uptime);
}

domain::Settings SettingsDialog::settings() const
{
    return domain::Settings{
        .refreshInterval = shownRefreshInterval(),
        .displayUnits =
            {
                .bytes = selectedValue<domain::ByteUnits>(m_byteUnitsComboBox),
                .networkRates = selectedValue<domain::NetworkRateUnits>(m_networkRateUnitsComboBox),
            },
        .dashboardPanels =
            {
                .cpu = m_cpuPanelCheckBox->isChecked(),
                .memory = m_memoryPanelCheckBox->isChecked(),
                .disk = m_diskPanelCheckBox->isChecked(),
                .network = m_networkPanelCheckBox->isChecked(),
                .uptime = m_uptimePanelCheckBox->isChecked(),
            },
        .logLevel = selectedValue<domain::LogLevel>(m_logLevelComboBox),
    };
}

std::chrono::milliseconds SettingsDialog::shownRefreshInterval() const
{
    return domain::nearestRefreshInterval(
        std::chrono::milliseconds{std::lround(m_refreshIntervalSpinBox->value() * kMillisecondsPerSecond)});
}

QDoubleSpinBox* SettingsDialog::refreshIntervalSpinBox() const
{
    return m_refreshIntervalSpinBox;
}

QComboBox* SettingsDialog::logLevelComboBox() const
{
    return m_logLevelComboBox;
}

QComboBox* SettingsDialog::byteUnitsComboBox() const
{
    return m_byteUnitsComboBox;
}

QComboBox* SettingsDialog::networkRateUnitsComboBox() const
{
    return m_networkRateUnitsComboBox;
}

QCheckBox* SettingsDialog::cpuPanelCheckBox() const
{
    return m_cpuPanelCheckBox;
}

QCheckBox* SettingsDialog::memoryPanelCheckBox() const
{
    return m_memoryPanelCheckBox;
}

QCheckBox* SettingsDialog::diskPanelCheckBox() const
{
    return m_diskPanelCheckBox;
}

QCheckBox* SettingsDialog::networkPanelCheckBox() const
{
    return m_networkPanelCheckBox;
}

QCheckBox* SettingsDialog::uptimePanelCheckBox() const
{
    return m_uptimePanelCheckBox;
}

QDialogButtonBox* SettingsDialog::buttonBox() const
{
    return m_buttonBox;
}

} // namespace verdandi::ui
