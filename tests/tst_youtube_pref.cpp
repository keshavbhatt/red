#include "core/theme/youtube_pref.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestYouTubePref : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void setsBitsAndKeepsOtherFields()
    {
        const QString device = u"f6=40000000&tz=Asia.Calcutta&f7=100"_s;
        QCOMPARE(themeFromYouTubePref(device), Theme::System);
        QCOMPARE(youTubePrefForTheme(device, Theme::Light), u"f6=40080000&tz=Asia.Calcutta&f7=100"_s);
        QCOMPARE(youTubePrefForTheme(device, Theme::Dark), u"f6=40000400&tz=Asia.Calcutta&f7=100"_s);
        QCOMPARE(youTubePrefForTheme(u"f6=40080000&tz=x"_s, Theme::Dark), u"f6=40000400&tz=x"_s);
        QCOMPARE(youTubePrefForTheme(u"f6=40080000&tz=x"_s, Theme::System), u"f6=40000000&tz=x"_s);
        QCOMPARE(themeFromYouTubePref(u"f6=40080000"_s), Theme::Light);
        QCOMPARE(themeFromYouTubePref(u"f6=400"_s), Theme::Dark);
    }

    void worksFromNothing()
    {
        QCOMPARE(youTubePrefForTheme(QString(), Theme::Light), u"f6=80000"_s);
        QCOMPARE(youTubePrefForTheme(QString(), Theme::System), QString());
        QCOMPARE(youTubePrefForTheme(u"tz=UTC"_s, Theme::Dark), u"f6=400&tz=UTC"_s);
        QCOMPARE(youTubePrefForTheme(u"f6=80000"_s, Theme::System), QString());
        QCOMPARE(youTubePrefForTheme(u"f6=zz"_s, Theme::Dark), u"f6=400"_s);
    }
};

QTEST_GUILESS_MAIN(TestYouTubePref)
#include "tst_youtube_pref.moc"
