#include "markdown_html.h"
#include "theme.h"

QString wrapMarkdownHtml(const QString &bodyHtml, bool dark)
{
    return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>" + Theme::markdownStyleSheet(dark)
           + "</style></head><body>" + bodyHtml + "</body></html>";
}
