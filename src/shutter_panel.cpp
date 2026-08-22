#include "shutter_panel.h"

#include <QVBoxLayout>

ShutterPanel::ShutterPanel(QWidget *content, int side, QWidget *parent)
    : QWidget(parent)
    , m_side(side)
    , m_content(content)
    , m_open(true)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_content);
}

void ShutterPanel::setOpen(bool open)
{
    if (m_open == open)
        return;
    m_open = open;

    // Hiding the whole panel (not just the content) makes the splitter give
    // its space to the editor instead of leaving a dead collapsed strip.
    setVisible(open);

    if (open)
        emit opened();
    else
        emit closed();
}

bool ShutterPanel::isOpen() const
{
    return m_open;
}

QWidget *ShutterPanel::content() const
{
    return m_content;
}
