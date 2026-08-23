#include "toggle_switch.h"
#include "theme.h"

#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
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

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, 0, width(), height());
    const qreal r = track.height() / 2.0;

    // Mono-tone track (the app's existing blue-gray accent, not a
    // stoplight-style green/red) — same color in both light and dark mode.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::Accent));
    p.drawRoundedRect(track, r, r);

    const qreal knobDiameter = track.height() - 4;
    const qreal knobX = isChecked() ? track.width() - knobDiameter - 2 : 2;
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(knobX, 2, knobDiameter, knobDiameter));

    if (hasFocus()) {
        QStyleOptionFocusRect option;
        option.initFrom(this);
        option.rect = rect().adjusted(1, 1, -1, -1);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &p, this);
    }
}
