#include "core/changelog.h"

#include <QFile>
#include <QRegularExpression>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

namespace {
const QString kDoc =
    u"# Changelog\n\nintro\n\n## [10.1.0] — unreleased\n\nNew stuff.\n\n### Added\n- **A**: one\n\n"
    "## [10.0.0] - 2026-09-20\n\nRewrite.\n\n### Removed\n- old\n\n[10.1.0]: https://x/compare\n[10.0.0]: https://x/tag\n"_s;
}

class TestChangelog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sectionWithSuffixStopsAtNextHeading()
    {
        QCOMPARE(changelogSection(kDoc, u"10.1.0"_s), u"New stuff.\n\n### Added\n- **A**: one"_s);
    }

    void lastSectionRunsToEndWithoutLinkReferences()
    {
        QCOMPARE(changelogSection(kDoc, u"10.0.0"_s), u"Rewrite.\n\n### Removed\n- old"_s);
    }

    void headingVariants()
    {
        QCOMPARE(changelogSection(u"## [v2.0.0]\nbody\n"_s, u"2.0.0"_s), u"body"_s);
        QCOMPARE(changelogSection(u"## [2.0.0]\r\nline one\r\nline two\r\n"_s, u"2.0.0"_s),
                 u"line one\nline two"_s);
    }

    void missingVersionIsEmpty()
    {
        QVERIFY(changelogSection(kDoc, u"9.0.0"_s).isEmpty());
        QVERIFY(changelogSection(kDoc, u"10.0.0-test"_s).isEmpty());
        QVERIFY(changelogSection(QString(), u"10.0.0"_s).isEmpty());
    }

    void releasesInDocumentOrderWithDates()
    {
        const QList<ChangelogRelease> releases = changelogReleases(kDoc);
        QCOMPARE(releases.size(), 2);
        QCOMPARE(releases.at(0).version, u"10.1.0"_s);
        QVERIFY(releases.at(0).date.isEmpty()); // unreleased
        QCOMPARE(releases.at(1).version, u"10.0.0"_s);
        QCOMPARE(releases.at(1).date, u"2026-09-20"_s);
        // A v prefix, CRLF, and link-reference lines that are not headings.
        const QList<ChangelogRelease> other =
            changelogReleases(u"## [v2.0.0] - 2026-01-02\r\n[2.0.0]: x\r\n"_s);
        QCOMPARE(other.size(), 1);
        QCOMPARE(other.at(0).version, u"2.0.0"_s);
        QCOMPARE(other.at(0).date, u"2026-01-02"_s);
        QVERIFY(changelogReleases(QString()).isEmpty());
    }

    void realChangelogListsEveryRelease()
    {
        QFile file(QString::fromLatin1(RED_CHANGELOG_PATH));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QList<ChangelogRelease> releases = changelogReleases(QString::fromUtf8(file.readAll()));
        QVERIFY(releases.size() >= 5);
        QCOMPARE(releases.first().version, QString::fromLatin1(RED_VERSION)); // newest first
        QCOMPARE(releases.last().version, u"10.0.0"_s);
    }

    void realChangelogHasNotesForThisVersion()
    {
        QFile file(QString::fromLatin1(RED_CHANGELOG_PATH));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString text = QString::fromUtf8(file.readAll());
        QVERIFY2(!changelogSection(text, QString::fromLatin1(RED_VERSION)).isEmpty(),
                 "CHANGELOG.md has no section for the project version");
        // Owner's rule (CLAUDE.md): accounts and licensing never appear in
        // user-facing release notes, and this file feeds the What's new dialog.
        static const QRegularExpression kForbidden(
            u"licen[cs]e|licensing|\\baccounts?\\b|checkout|evaluation|\\btrial\\b|Red Pro|activation"_s,
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = kForbidden.match(text);
        QVERIFY2(!m.hasMatch(), qPrintable(u"CHANGELOG.md mentions licensing: "_s + m.captured(0)));
    }
};

QTEST_GUILESS_MAIN(TestChangelog)
#include "tst_changelog.moc"
