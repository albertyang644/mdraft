#include "preview_widget.h"
#include "markdown_html.h"
#include "theme.h"

#include <QVBoxLayout>
#include <QLabel>

#ifdef MDRAFT_HAVE_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>
#include <QProcess>
#include <QTimer>

namespace {
class LocalOnlyRequestInterceptor final : public QWebEngineUrlRequestInterceptor
{
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        const QString scheme = info.requestUrl().scheme().toLower();
        if (scheme != QStringLiteral("about")
            && scheme != QStringLiteral("data")
            && scheme != QStringLiteral("file")
            && scheme != QStringLiteral("qrc")) {
            info.block(true);
        }
    }
};
}
#endif

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
#ifdef MDRAFT_HAVE_WEBENGINE
    , m_profile(nullptr)
    , m_view(nullptr)
    , m_pandocProcess(nullptr)
    , m_conversionTimeout(new QTimer(this))
    , m_darkMode(false)
#endif

{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);

    m_placeholder = new QWidget(this);
    auto *pl = new QVBoxLayout(m_placeholder);
#ifdef MDRAFT_HAVE_WEBENGINE
    m_placeholderLabel = new QLabel(tr("Preview panel is closed.\nOpen it to render."), m_placeholder);
    m_conversionTimeout->setSingleShot(true);
    m_conversionTimeout->setInterval(30000);
    connect(m_conversionTimeout, &QTimer::timeout, this, [this]() {
        if (!m_pandocProcess)
            return;
        QProcess *process = m_pandocProcess;
        process->disconnect(this);
        process->kill();
        finishConversion(process, "<pre>" + m_convertingSource.toHtmlEscaped() + "</pre>");
    });
#else
    m_placeholderLabel = new QLabel(tr("Rendered preview is unavailable in this build."), m_placeholder);
#endif
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

    m_profile = new QWebEngineProfile(this);
    auto *interceptor = new LocalOnlyRequestInterceptor(m_profile);
    m_profile->setUrlRequestInterceptor(interceptor);

    m_view = new QWebEngineView(this);
    m_view->setPage(new QWebEnginePage(m_profile, m_view));
    m_view->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    m_view->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    m_view->page()->setBackgroundColor(m_darkMode ? QColor(Theme::DarkEditor) : Qt::white); // matches the preview's CSS background

    // The WebView's first paint on X11/GL tends to flash black/blank before
    // content lands. Keep it hidden until content has actually loaded once,
    // so opening the panel never shows that transient frame.
    m_view->setVisible(false);
    connect(m_view, &QWebEngineView::loadFinished, this, [this](bool) {
        if (m_view && !m_view->isVisible())
            m_view->setVisible(true);
    });

    connect(m_view->page(), &QWebEnginePage::scrollPositionChanged, this,
            [this](const QPointF &position) {
        const qreal span = m_view->page()->contentsSize().height() - m_view->height();
        if (span <= 1.0)
            return;
        const qreal fraction = qBound(0.0, position.y() / span, 1.0);
        // One scrollTo can emit several scrollPositionChanged events, so a
        // one-shot flag would let the tail of our own scroll look like user
        // input and feed back. Compare against what we last drove instead.
        if (m_appliedFraction >= 0.0 && qAbs(fraction - m_appliedFraction) < 0.01)
            return;
        m_appliedFraction = -1.0;
        emit scrolled(fraction);
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
        // The latest source is already in m_pendingMarkdown. Completion always
        // compares against it before deciding whether another run is needed.
        return;
    }

    m_convertingSource = markdown;
    m_pandocProcess = new QProcess(this);

    QProcess *process = m_pandocProcess;

    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
        if (process != m_pandocProcess)
            return;
        if (status == QProcess::NormalExit && exitCode == 0)
            finishConversion(process, QString::fromUtf8(process->readAllStandardOutput()));
        else
            finishConversion(process, "<pre>" + m_convertingSource.toHtmlEscaped() + "</pre>");
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
        // pandoc missing or failed to start: fall back to a raw escaped dump
        // so the preview still shows something.
        if (process->state() != QProcess::NotRunning)
            process->kill();
        finishConversion(process, "<pre>" + m_convertingSource.toHtmlEscaped() + "</pre>");
    });

    process->start("pandoc", {"--from=gfm", "--to=html"});
    process->write(markdown.toUtf8());
    process->closeWriteChannel();
    m_conversionTimeout->start();
#else
    Q_UNUSED(markdown);
#endif
}

#ifdef MDRAFT_HAVE_WEBENGINE
void PreviewWidget::finishConversion(QProcess *process, const QString &bodyHtml)
{
    if (!process || process != m_pandocProcess)
        return;

    m_conversionTimeout->stop();
    process->disconnect(this);
    m_lastBodyHtml = bodyHtml;
    renderHtml(wrapMarkdownHtml(bodyHtml, m_darkMode));
    m_lastRenderedSource = m_convertingSource;
    m_pandocProcess = nullptr;
    process->deleteLater();

    if (m_pendingMarkdown != m_lastRenderedSource)
        convertAndRender(m_pendingMarkdown);
}
#endif

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
        m_pandocProcess->deleteLater();
        m_pandocProcess = nullptr;
        m_conversionTimeout->stop();
    }
    if (m_view) {
        delete m_view;
        m_view = nullptr;
        m_placeholder->show();
    }
    if (m_profile) {
        m_profile->setUrlRequestInterceptor(nullptr);
        delete m_profile;
        m_profile = nullptr;
    }
#endif
}

void PreviewWidget::setScrollFraction(qreal fraction)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (!m_view)
        return;
    m_appliedFraction = qBound(0.0, fraction, 1.0);
    const QString js = QStringLiteral(
        "(function(){var e=document.documentElement;"
        "var m=Math.max(0,(e.scrollHeight||0)-(window.innerHeight||0));"
        "window.scrollTo(0,m*%1);})();").arg(qBound(0.0, fraction, 1.0));
    // ApplicationWorld, not the main world: page JavaScript is disabled for
    // safety, and an isolated world still runs (and shares the DOM) while a
    // document's own scripts stay blocked.
    m_view->page()->runJavaScript(js, QWebEngineScript::ApplicationWorld);
#else
    Q_UNUSED(fraction);
#endif
}

void PreviewWidget::setDarkMode(bool dark)
{
#ifdef MDRAFT_HAVE_WEBENGINE
    if (m_darkMode == dark)
        return;
    m_darkMode = dark;
    if (m_view) {
        m_view->page()->setBackgroundColor(m_darkMode ? QColor(Theme::DarkEditor) : Qt::white);
        if (!m_lastBodyHtml.isEmpty())
            renderHtml(wrapMarkdownHtml(m_lastBodyHtml, m_darkMode));
    }
#else
    Q_UNUSED(dark);
#endif
}
