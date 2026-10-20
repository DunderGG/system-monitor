#include "ui/network_view.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <utility>

#include <QCheckBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>

#include "ui/adapter_card.h"
#include "ui/dashboard_formatting.h"
#include "ui/network_formatting.h"
#include "ui/performance_readouts.h"
#include "ui/readout_grid.h"
#include "ui/view_metrics.h"

namespace sysmon::ui
{

namespace
{

constexpr int kConnectivityColumns = 4;

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

NetworkView::NetworkView(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kViewMargin, kViewMargin, kViewMargin, kViewMargin);
    layout->setSpacing(kSectionSpacing);

    auto* titleLabel = new QLabel("Network", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(kTitlePointSize);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* connectivitySection = new QVBoxLayout();
    connectivitySection->setSpacing(kItemSpacing);
    connectivitySection->addWidget(sectionLabel("Connectivity", this));
    m_connectivityGrid = new ReadoutGrid(kConnectivityColumns, this);
    m_connectivityGrid->setReadouts(placeholderReadouts(connectivityReadouts({}, std::nullopt)));
    connectivitySection->addWidget(m_connectivityGrid);
    layout->addLayout(connectivitySection);

    auto* adaptersHeader = new QHBoxLayout();
    adaptersHeader->addWidget(sectionLabel("Adapters", this));
    adaptersHeader->addStretch();
    m_showAllCheckBox = new QCheckBox("Show all adapters", this);
    m_showAllCheckBox->setToolTip("Also list the adapters that Windows hides, such as the WAN Miniports and the "
                                  "Wi-Fi Direct adapters, and virtual adapters that are not bound to IP, which "
                                  "cannot carry IP traffic. They are listed after the others.");
    adaptersHeader->addWidget(m_showAllCheckBox);
    layout->addLayout(adaptersHeader);

    m_placeholderLabel = new QLabel("--", this);
    m_placeholderLabel->setWordWrap(true);
    layout->addWidget(m_placeholderLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    // The cards always fit the width, so only vertical scrolling is needed.
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* cardList = new QWidget(m_scrollArea);
    m_cardLayout = new QVBoxLayout(cardList);
    m_cardLayout->setContentsMargins(0, 0, 0, 0);
    m_cardLayout->setSpacing(kSectionSpacing);
    m_cardLayout->addStretch();
    // Placed before the first card that only "Show all adapters" lists.
    m_hiddenHeading = sectionLabel("Hidden adapters", cardList);
    m_hiddenHeading->hide();
    m_scrollArea->setWidget(cardList);
    layout->addWidget(m_scrollArea, 1);

    connect(m_showAllCheckBox, &QCheckBox::toggled, this, [this] {
        if (isVisible()) {
            refresh();
        }
    });
}

void NetworkView::updateSnapshot(const domain::SystemSnapshot& snapshot)
{
    ++m_recordedCount;
    m_connectivity = snapshot.connectivity;
    m_networkProfiles = snapshot.networkProfiles;
    m_latest = snapshot.networks;
    m_history.record(snapshot.networks);
    // Hidden widgets need no update; showEvent() catches up.
    if (isVisible()) {
        refresh();
    }
}

void NetworkView::setShowAllAdapters(bool showAll)
{
    m_showAllCheckBox->setChecked(showAll);
}

bool NetworkView::showsAllAdapters() const
{
    return m_showAllCheckBox->isChecked();
}

void NetworkView::setSampleInterval(std::chrono::milliseconds interval)
{
    m_sampleInterval = interval;
    for (auto* card : m_cards) {
        card->setSampleInterval(interval);
    }
}

std::chrono::milliseconds NetworkView::sampleInterval() const
{
    return m_sampleInterval;
}

void NetworkView::setDisplayUnits(domain::DisplayUnits units)
{
    m_displayUnits = units;
    for (auto* card : m_cards) {
        card->setDisplayUnits(units);
    }
}

domain::DisplayUnits NetworkView::displayUnits() const
{
    return m_displayUnits;
}

const std::vector<uint64_t>& NetworkView::listedInterfaceIds() const
{
    return m_listedIds;
}

AdapterCard* NetworkView::adapterCard(std::size_t index) const
{
    assert(index < m_cards.size() && "adapter card index out of range");
    return m_cards[index];
}

QString NetworkView::placeholderText() const
{
    return m_placeholderLabel->text();
}

ReadoutGrid* NetworkView::connectivityReadoutGrid() const
{
    return m_connectivityGrid;
}

void NetworkView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void NetworkView::refresh()
{
    if (m_recordedCount == 0) {
        return;
    }

    m_connectivityGrid->setReadouts(connectivityReadouts(m_connectivity, m_networkProfiles));

    // Without a successful read, keep the cards from the last one.
    if (m_latest) {
        const auto listed = listedAdapters(*m_latest, showsAllAdapters());
        std::vector<uint64_t> ids;
        ids.reserve(listed.size());
        for (const auto* adapter : listed) {
            ids.push_back(adapter->interfaceId);
        }
        // listedAdapters() puts the adapters listed by default first.
        const auto firstHidden = std::ranges::find_if_not(
            listed, [](const domain::NetworkSample* adapter) { return isListedByDefault(*adapter); });
        arrangeCards(ids, static_cast<std::size_t>(firstHidden - listed.begin()));
        for (std::size_t index = 0; index < listed.size(); ++index) {
            m_cards[index]->setAdapter(*listed[index], m_networkProfiles);
        }
    } else {
        for (auto* card : m_cards) {
            card->setUnavailable();
        }
    }
    for (std::size_t index = 0; index < m_cards.size(); ++index) {
        m_cards[index]->setThroughput(m_history.find(m_listedIds[index]), m_recordedCount);
    }

    if (m_cards.empty()) {
        if (!m_latest) {
            m_placeholderLabel->setText(kNotAvailable);
        } else if (m_latest->empty()) {
            m_placeholderLabel->setText("No network adapters");
        } else {
            // Every adapter is one that only "Show all adapters" lists.
            m_placeholderLabel->setText("No adapters to list. Select Show all adapters to list the hidden ones.");
        }
    }
    m_placeholderLabel->setVisible(m_cards.empty());
}

void NetworkView::arrangeCards(const std::vector<uint64_t>& interfaceIds, std::size_t hiddenStart)
{
    if (interfaceIds == m_listedIds && hiddenStart == m_hiddenStart) {
        return;
    }

    // Reuse the card of every adapter still listed, so it keeps its place in
    // the scroll position and its widgets are not rebuilt.
    std::unordered_map<uint64_t, AdapterCard*> previous;
    for (std::size_t index = 0; index < m_cards.size(); ++index) {
        previous.emplace(m_listedIds[index], m_cards[index]);
        m_cardLayout->removeWidget(m_cards[index]);
    }
    m_cardLayout->removeWidget(m_hiddenHeading);

    std::vector<AdapterCard*> cards;
    cards.reserve(interfaceIds.size());
    for (const uint64_t interfaceId : interfaceIds) {
        if (const auto it = previous.find(interfaceId); it != previous.end()) {
            cards.push_back(it->second);
            previous.erase(it);
        } else {
            auto* card = new AdapterCard(kHistoryCapacity, m_scrollArea->widget());
            card->setSampleInterval(m_sampleInterval);
            card->setDisplayUnits(m_displayUnits);
            cards.push_back(card);
        }
    }
    for (auto& [interfaceId, card] : previous) {
        card->hide();
        card->deleteLater();
    }

    // Insert before the trailing stretch, in display order, with the heading
    // before the first card that only "Show all adapters" lists.
    for (std::size_t index = 0; index < cards.size(); ++index) {
        m_cardLayout->insertWidget(m_cardLayout->count() - 1, cards[index]);
        cards[index]->show();
    }
    const bool hasHiddenCards = hiddenStart < cards.size();
    if (hasHiddenCards) {
        m_cardLayout->insertWidget(static_cast<int>(hiddenStart), m_hiddenHeading);
    }
    m_hiddenHeading->setVisible(hasHiddenCards);
    m_listedIds = interfaceIds;
    m_hiddenStart = hiddenStart;
    m_cards = std::move(cards);
}

std::optional<std::size_t> NetworkView::hiddenAdaptersHeadingPosition() const
{
    const int position = m_cardLayout->indexOf(m_hiddenHeading);
    if (m_hiddenHeading->isHidden() || position < 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(position);
}

} // namespace sysmon::ui
