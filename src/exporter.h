#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>
#include <functional>

class QObject;

/**
 * On-demand export to HTML, PDF and LaTeX.
 *
 * <HARD_CONTRACT> Export is invoked only by explicit user action and never
 * blocks the editing hot path. Tool detection (pandoc/latex) is cheap and
 * does not run at startup. </HARD_CONTRACT>
 */
class Exporter
{
public:
    using Completion = std::function<void(bool ok, const QString &error)>;

    static void exportMarkdown(const QString &markdown, const QString &dstPath,
                               const QString &format, QObject *context,
                               Completion completion);
};

#endif // EXPORTER_H
