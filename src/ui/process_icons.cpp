#include "ui/process_icons.h"

#include <array>
#include <functional>

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>

namespace sysmon::ui
{

namespace
{

// The design grid, in device-independent pixels: the default item icon size.
constexpr int kIconSize = 16;
constexpr std::array kDevicePixelRatios{1.0, 1.5, 2.0};

constexpr QColor kDeniedColor{208, 138, 16};
constexpr QColor kNeverOpenedColor{61, 127, 201};
constexpr QColor kUnavailableColor{140, 140, 140};
constexpr qreal kStrokeWidth = 1.6;

using Glyph = std::function<void(QPainter&)>;

QIcon paintIcon(const Glyph& glyph)
{
    QIcon icon;
    for (const qreal ratio : kDevicePixelRatios) {
        QPixmap pixmap(QSize(kIconSize, kIconSize) * ratio);
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        if (glyph) {
            QPainter painter(&pixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            glyph(painter);
        }
        icon.addPixmap(pixmap);
    }
    return icon;
}

void paintPadlock(QPainter& painter)
{
    QPainterPath shackle;
    shackle.moveTo(5.0, 7.5);
    shackle.lineTo(5.0, 5.0);
    shackle.arcTo(QRectF(5.0, 2.0, 6.0, 6.0), 180.0, -180.0);
    shackle.lineTo(11.0, 7.5);
    painter.setPen(QPen(kDeniedColor, kStrokeWidth, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shackle);

    painter.setPen(Qt::NoPen);
    painter.setBrush(kDeniedColor);
    painter.drawRoundedRect(QRectF(3.0, 7.0, 10.0, 7.5), 1.5, 1.5);
}

void paintShield(QPainter& painter)
{
    QPainterPath shield;
    shield.moveTo(8.0, 1.5);
    shield.lineTo(13.5, 3.5);
    shield.lineTo(13.5, 8.0);
    shield.cubicTo(13.5, 11.5, 11.0, 13.5, 8.0, 15.0);
    shield.cubicTo(5.0, 13.5, 2.5, 11.5, 2.5, 8.0);
    shield.lineTo(2.5, 3.5);
    shield.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(kNeverOpenedColor);
    painter.drawPath(shield);
}

void paintBarredCircle(QPainter& painter)
{
    painter.setPen(QPen(kUnavailableColor, kStrokeWidth, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QRectF(2.5, 2.5, 11.0, 11.0));
    painter.drawLine(QPointF(5.0, 8.0), QPointF(11.0, 8.0));
}

} // namespace

QIcon detailsStatusIcon(domain::ProcessDetailsStatus status)
{
    switch (status) {
        case domain::ProcessDetailsStatus::AccessDenied:
            return paintIcon(paintPadlock);
        case domain::ProcessDetailsStatus::NotQueried:
            return paintIcon(paintShield);
        case domain::ProcessDetailsStatus::Unavailable:
            return paintIcon(paintBarredCircle);
        case domain::ProcessDetailsStatus::Available:
            break;
    }
    return paintIcon(nullptr);
}

} // namespace sysmon::ui
