#include "core/changelog.h"

#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace red::core {

QString changelogSection(const QString& markdown, const QString& version)
{
    static const QRegularExpression kHeading(u"^## \\[v?([^\\]]+)\\]"_s);
    static const QRegularExpression kLinkReference(u"^\\[[^\\]]+\\]:\\s*\\S+"_s);
    QStringList body;
    bool inside = false;
    for (QString line : markdown.split(u'\n')) {
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        if (line.startsWith(u"## "_s)) {
            if (inside) {
                break;
            }
            const QRegularExpressionMatch m = kHeading.match(line);
            inside = m.hasMatch() && m.captured(1).trimmed() == version;
            continue;
        }
        if (inside && !kLinkReference.match(line).hasMatch()) {
            body << line;
        }
    }
    while (!body.isEmpty() && body.first().trimmed().isEmpty()) {
        body.removeFirst();
    }
    while (!body.isEmpty() && body.last().trimmed().isEmpty()) {
        body.removeLast();
    }
    return body.join(u'\n');
}

} // namespace red::core
