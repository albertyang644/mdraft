#ifndef MARKDOWN_HTML_H
#define MARKDOWN_HTML_H

#include <QString>

// Wraps pandoc-generated body HTML in a standalone document using mdraft's
// preview styling, so the live preview panel and "Export HTML" render the
// same way instead of the preview looking polished and the export coming
// out as bare, unstyled pandoc paragraphs.
QString wrapMarkdownHtml(const QString &bodyHtml, bool dark);

#endif // MARKDOWN_HTML_H
