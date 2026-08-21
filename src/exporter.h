#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>

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
    static bool exportTo(const QString &srcPath, const QString &dstPath, const QString &format, QString &error);
    static bool htmlToPdf(const QString &html, const QString &dstPath, QString &error);

private:
    static QString runProcess(const QString &program, const QStringList &args, bool *ok, QString *err);
};

#endif // EXPORTER_H
