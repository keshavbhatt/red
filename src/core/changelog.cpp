#include "core/changelog.h"

#include <QRegularExpression>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace red::core {

QList<ChangelogRelease> changelogReleases(const QString& markdown)
{
    static const QRegularExpression kHeading(u"^## \\[v?([^\\]]+)\\](.*)$"_s);
    static const QRegularExpression kDate(u"(\\d{4}-\\d{2}-\\d{2})"_s);
    QList<ChangelogRelease> releases;
    for (QString line : markdown.split(u'\n')) {
        if (line.endsWith(u'\r')) {
            line.chop(1);
        }
        const QRegularExpressionMatch m = kHeading.match(line);
        if (!m.hasMatch()) {
            continue;
        }
        const QRegularExpressionMatch date = kDate.match(m.captured(2));
        releases.append(
            {.version = m.captured(1).trimmed(), .date = date.hasMatch() ? date.captured(1) : QString()});
    }
    return releases;
}

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
