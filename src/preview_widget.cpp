#include "preview_widget.h"
#include "markdown_html.h"

#include <QVBoxLayout>
#include <QLabel>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QPrinter>
#include <QTemporaryFile>
#include <QProcess>
#endif

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
#ifdef MDRAFT_HAVE_WEBENGINE
    , m_view(nullptr)
    , m_lastPdfPath()
    , m_pandocProcess(nullptr)
    , m_conversionPending(false)
    , m_darkMode(false)
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
    m_view->page()->setBackgroundColor(m_darkMode ? QColor("#1e1e1e") : Qt::white); // matches the preview's CSS background

    // The WebView's first paint on X11/GL tends to flash black/blank before
    // content lands. Keep it hidden until content has actually loaded once,
    // so opening the panel never shows that transient frame.
    m_view->setVisible(false);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool) {
        if (m_view && !m_view->isVisible())
            m_view->setVisible(true);
    });

    m_layout->addWidget(m_view);
    m_placeholder->hide();

    if (!m_lastBodyHtml.isEmpty())
        renderHtml(wrapMarkdownHtml(m_lastBodyHtml, m_darkMode)); // instant re-show of the last render, no flash

    // Only shell out to pandoc again if the source actually changed while
    // the panel was closed (or this is the very first render).
    if (m_pendingMarkdown != m_lastRenderedSource || m_lastHtml.isEmpty())
        convertAndRender(m_pendingMarkdown);
#endif
}

void PreviewWidget::setMarkdown(const QString &markdown)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    m_pendingMarkdown = markdown;
    if (m_view && markdown != m_lastRenderedSource)
        convertAndRender(markdown);
#else
    Q_UNUSED(markdown);
#endif
}

void PreviewWidget::convertAndRender(const QString &markdown)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_pandocProcess) {
        // A conversion is already in flight; re-run with the latest text once
        // it finishes instead of piling up processes.
        m_conversionPending = true;
        return;
    }

    m_convertingSource = markdown;
    m_pandocProcess = new QProcess(this);

    auto finishConversion = [this](const QString &bodyHtml) {
        m_lastBodyHtml = bodyHtml;
        renderHtml(wrapMarkdownHtml(bodyHtml, m_darkMode));
        m_lastRenderedSource = m_convertingSource;
        m_pandocProcess->deleteLater();
        m_pandocProcess = nullptr;
        if (m_conversionPending) {
            m_conversionPending = false;
            convertAndRender(m_pendingMarkdown);
        }
    };

    connect(m_pandocProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, finishConversion](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0)
            finishConversion(QString::fromUtf8(m_pandocProcess->readAllStandardOutput()));
        else
            finishConversion("<pre>" + m_convertingSource.toHtmlEscaped() + "</pre>");
    });
    connect(m_pandocProcess, &QProcess::errorOccurred, this, [this, finishConversion](QProcess::ProcessError) {
        // pandoc missing or failed to start: fall back to a raw escaped dump
        // so the preview still shows something.
        finishConversion("<pre>" + m_convertingSource.toHtmlEscaped() + "</pre>");
    });

    m_pandocProcess->start("pandoc", {"--from=gfm", "--to=html"});
    m_pandocProcess->write(markdown.toUtf8());
    m_pandocProcess->closeWriteChannel();
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
    m_lastHtml = html;
    if (m_view)
        m_view->setHtml(html, QUrl("about:blank"));
#else
    Q_UNUSED(html);
#endif
}

void PreviewWidget::teardownPreview()
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_pandocProcess) {
        // Disconnect first: finished()/errorOccurred() would otherwise still
        // fire (posted, not synchronous) and call back into this widget's
        // half-torn-down state.
        m_pandocProcess->disconnect(this);
        m_pandocProcess->kill();
        m_pandocProcess->waitForFinished(200);
        m_pandocProcess->deleteLater();
        m_pandocProcess = nullptr;
        m_conversionPending = false;
    }
    if (m_view) {
        delete m_view;
        m_view = nullptr;
        m_placeholder->show();
    }
#endif
}

void PreviewWidget::setDarkMode(bool dark)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    if (m_view) {
        m_view->page()->setBackgroundColor(m_darkMode ? QColor("#1e1e1e") : Qt::white);
        if (!m_lastBodyHtml.isEmpty())
            renderHtml(wrapMarkdownHtml(m_lastBodyHtml, m_darkMode));
    }
#else
    Q_UNUSED(dark);
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
