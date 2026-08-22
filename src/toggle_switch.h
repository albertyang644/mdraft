#ifndef TOGGLE_SWITCH_H
#define TOGGLE_SWITCH_H

#include <QAbstractButton>

// A small iOS-style sliding toggle switch. Checked = knob on the right.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

#endif // TOGGLE_SWITCH_H
