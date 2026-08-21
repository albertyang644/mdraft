#ifndef PREVIEW_WIDGET_H
#define PREVIEW_WIDGET_H

#include <QWidget>

class QVBoxLayout;
class QLabel;

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

private:
    void ensureWebView();
    void renderHtml(const QString &html);

    QVBoxLayout *m_layout;
    QWidget *m_placeholder;
    QLabel *m_placeholderLabel;

#ifdef MDRAFT_HAVE_WEBENGINE
    QWebEngineView *m_view;
    QString m_lastHtml;
    QString m_lastPdfPath;
#endif
};

#endif // PREVIEW_WIDGET_H
