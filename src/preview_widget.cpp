#include "preview_widget.h"

#include <QVBoxLayout>
#include <QLabel>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QPrinter>
#include <QTemporaryFile>
#endif

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
#ifdef MDRAFT_HAVE_WEBENGINE
    , m_view(nullptr)
    , m_lastPdfPath()
#endif
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_placeholder = new QWidget(this);
    auto *pl = new QVBoxLayout(m_placeholder);
    m_placeholderLabel = new QLabel(tr("Preview panel is closed.\nOpen it to render."), m_placeholder);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet("color: gray;");
    pl->addWidget(m_placeholderLabel);
    m_layout->addWidget(m_placeholder);
}

PreviewWidget::~PreviewWidget()
{
    teardownPreview();
}

void PreviewWidget::ensureWebView()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_view)
        return;
    m_view = new QWebEngineView(this);
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    m_layout->addWidget(m_view);
    m_placeholder->hide();
    if (!m_lastHtml.isEmpty())
        m_view->setHtml(m_lastHtml, QUrl("about:blank"));
#endif
}

void PreviewWidget::setMarkdown(const QString &markdown)
{
    // Build the HTML wrapper regardless of whether the WebView is open, so
    // opening the panel later renders the latest content.
#ifdef MDRAFT_HAVE_WEBENGINE
    QString html = "<html><head><style>body{font-family:sans-serif}"
                   "pre{white-space:pre-wrap;padding:12px;}</style></head><body>";
    html += "<pre>" + markdown.toHtmlEscaped() + "</pre></body></html>";
    m_lastHtml = html;
    if (m_view)
        m_view->setHtml(html, QUrl("about:blank"));
#else
    Q_UNUSED(markdown);
#endif
}

void PreviewWidget::openPreview()
{
    ensureWebView();
}

bool PreviewWidget::isPreviewOpen() const
{
#ifdef MDRAFT_HAVE_WEBENGINE
    return m_view != nullptr;
#else
    return false;
#endif
}

void PreviewWidget::renderHtml(const QString &html)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_view)
        m_view->setHtml(html, QUrl("about:blank"));
#else
    Q_UNUSED(html);
#endif
}

void PreviewWidget::teardownPreview()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_view) {
        delete m_view;
        m_view = nullptr;
        m_placeholder->show();
    }
#endif
}

QString PreviewWidget::pdfFilePath() const
{
#ifdef MDRAFT_HAVE_WEBENGINE
    return m_lastPdfPath;
#else
    return QString();
#endif
}
