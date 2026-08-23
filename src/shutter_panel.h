#ifndef SHUTTER_PANEL_H
#define SHUTTER_PANEL_H

#include <QWidget>

/**
 * A thin wrapper that lets MainWindow fully show/hide a side panel (outline
 * or preview) inside the splitter. When closed, the panel takes zero space
 * and the splitter hands that space to the editor — there is no persistent
 * "collapsed bar" left behind. Toggling happens via the top-bar buttons in
 * MainWindow; this class only tracks state and forwards open/close signals
 * so the owner can manage its content's lifecycle (e.g. tearing down a
 * WebView when the preview panel closes).
 */
class ShutterPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ShutterPanel(QWidget *content, QWidget *parent = nullptr);

    void setOpen(bool open);
    bool isOpen() const;

signals:
    void opened();
    void closed();

private:
    bool m_open;
};

#endif // SHUTTER_PANEL_H
