#include "toggle_switch.h"

#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
    , m_foregroundColor(palette().color(QPalette::WindowText))
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Dark mode"));
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(38, 20);
}

void ToggleSwitch::setForegroundColor(const QColor &color)
{
    if (m_foregroundColor == color)
        return;
    m_foregroundColor = color;
    update();
}

QColor ToggleSwitch::foregroundColor() const
{
    return m_foregroundColor;
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0.75, 0.75, width() - 1.5, height() - 1.5);
    const qreal r = track.height() / 2.0;

    p.setPen(QPen(m_foregroundColor, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(track, r, r);

    const qreal knobDiameter = height() - 6;
    const qreal knobX = isChecked() ? width() - knobDiameter - 3 : 3;
    p.setPen(Qt::NoPen);
    p.setBrush(m_foregroundColor);
    p.drawEllipse(QRectF(knobX, 3, knobDiameter, knobDiameter));

    if (hasFocus()) {
        QPen focusPen(m_foregroundColor, 1);
        focusPen.setStyle(Qt::DotLine);
        p.setPen(focusPen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), r, r);
    }
}
