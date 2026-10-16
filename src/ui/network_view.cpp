#include "ui/network_view.h"

#include <cassert>
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
#include "ui/view_metrics.h"

namespace sysmon::ui
{

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

    auto* adaptersHeader = new QHBoxLayout();
    auto* adaptersLabel = new QLabel("Adapters", this);
    QFont adaptersFont = adaptersLabel->font();
    adaptersFont.setBold(true);
    adaptersLabel->setFont(adaptersFont);
    adaptersHeader->addWidget(adaptersLabel);
    adaptersHeader->addStretch();
    m_showAllCheckBox = new QCheckBox("Show all adapters", this);
    m_showAllCheckBox->setToolTip("Also list hidden virtual adapters that are not bound to IP, such as the WAN "
                                  "Miniports. They cannot carry IP traffic.");
    adaptersHeader->addWidget(m_showAllCheckBox);
    layout->addLayout(adaptersHeader);

    m_placeholderLabel = new QLabel("--", this);
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
    m_hasSnapshot = true;
    m_latest = snapshot.networks;
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

void NetworkView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void NetworkView::refresh()
{
    if (!m_hasSnapshot) {
        return;
    }

    // Without a successful read, keep the cards from the last one.
    if (m_latest) {
        const auto listed = listedAdapters(*m_latest, showsAllAdapters());
        std::vector<uint64_t> ids;
        ids.reserve(listed.size());
        for (const auto* adapter : listed) {
            ids.push_back(adapter->interfaceId);
        }
        arrangeCards(ids);
        for (std::size_t index = 0; index < listed.size(); ++index) {
            m_cards[index]->setAdapter(*listed[index]);
        }
    } else {
        for (auto* card : m_cards) {
            card->setUnavailable();
        }
    }

    if (m_cards.empty()) {
        m_placeholderLabel->setText(m_latest ? "No network adapters" : kNotAvailable);
    }
    m_placeholderLabel->setVisible(m_cards.empty());
}

void NetworkView::arrangeCards(const std::vector<uint64_t>& interfaceIds)
{
    if (interfaceIds == m_listedIds) {
        return;
    }

    // Reuse the card of every adapter still listed, so it keeps its place in
    // the scroll position and its widgets are not rebuilt.
    std::unordered_map<uint64_t, AdapterCard*> previous;
    for (std::size_t index = 0; index < m_cards.size(); ++index) {
        previous.emplace(m_listedIds[index], m_cards[index]);
        m_cardLayout->removeWidget(m_cards[index]);
    }

    std::vector<AdapterCard*> cards;
    cards.reserve(interfaceIds.size());
    for (const uint64_t interfaceId : interfaceIds) {
        if (const auto it = previous.find(interfaceId); it != previous.end()) {
            cards.push_back(it->second);
            previous.erase(it);
        } else {
            cards.push_back(new AdapterCard(m_scrollArea->widget()));
        }
    }
    for (auto& [interfaceId, card] : previous) {
        card->hide();
        card->deleteLater();
    }

    // Insert before the trailing stretch, in display order.
    for (std::size_t index = 0; index < cards.size(); ++index) {
        m_cardLayout->insertWidget(static_cast<int>(index), cards[index]);
        cards[index]->show();
    }
    m_listedIds = interfaceIds;
    m_cards = std::move(cards);
}

} // namespace sysmon::ui
