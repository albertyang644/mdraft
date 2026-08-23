#ifndef TOGGLE_SWITCH_H
#define TOGGLE_SWITCH_H

#include <QAbstractButton>
#include <QColor>

// A small iOS-style sliding toggle switch. Checked = knob on the right.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;
    void setForegroundColor(const QColor &color);
    QColor foregroundColor() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_foregroundColor;
};

#endif // TOGGLE_SWITCH_H
