#include "toggle_switch.h"

#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(38, 20);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, 0, width(), height());
    const qreal r = track.height() / 2.0;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#3fb46f"));
    p.drawRoundedRect(track, r, r);

    const qreal knobDiameter = track.height() - 4;
    const qreal knobX = isChecked() ? track.width() - knobDiameter - 2 : 2;
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(knobX, 2, knobDiameter, knobDiameter));
}
