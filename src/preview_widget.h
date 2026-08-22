#ifndef PREVIEW_WIDGET_H
#define PREVIEW_WIDGET_H

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QProcess;

#ifdef MDRAFT_HAVE_WEBENGINE
class QWebEngineView;
#endif

/**
 * The rendered preview panel.
 *
 * <HARD_CONTRACT> The panel itself is just a native container. Its WebView is
 * created lazily on first open and destroyed with the panel. There is no
 * background/hidden WebView kept warm. </HARD_CONTRACT>
 */
class PreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget *parent = nullptr);
    ~PreviewWidget() override;

    // Update the preview with raw markdown (debounced; renders if WebView open).
    void setMarkdown(const QString &markdown);

    // Called when the panel is opened -> lazily create the WebView.
    void openPreview();

    // Called when the panel is hidden/closed -> destroy the WebView.
    void teardownPreview();

    bool isPreviewOpen() const;

    // Render the current markdown to PDF via the WebView print (if available).
    QString pdfFilePath() const;

    // Switches the rendered HTML's own CSS between light/dark, independent of
    // the native Qt chrome (the WebView's content isn't a Qt widget, so the
    // app-wide stylesheet can't reach it).
    void setDarkMode(bool dark);

private:
    void ensureWebView();
    void renderHtml(const QString &html);
    void convertAndRender(const QString &markdown);

    QVBoxLayout *m_layout;
    QWidget *m_placeholder;
    QLabel *m_placeholderLabel;

#ifdef MDRAFT_HAVE_WEBENGINE
    QWebEngineView *m_view;
    QString m_lastHtml;
    QString m_lastBodyHtml;         // last pandoc output, unwrapped (re-wrapped on theme change)
    QString m_lastPdfPath;
    QString m_pendingMarkdown;      // latest source; re-converted once the running process exits
    QString m_lastRenderedSource;   // source text that produced m_lastHtml (skip reconversion if unchanged)
    QProcess *m_pandocProcess;
    QString m_convertingSource;     // source text the in-flight process was started with
    bool m_conversionPending;
    bool m_darkMode;
#endif
};

#endif // PREVIEW_WIDGET_H
