#include "services/netscape_cookies.h"

#include <QTemporaryFile>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::services;

class TestNetscapeCookies : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void filtersDomains()
    {
        QNetworkCookie yt("SID", "v");
        yt.setDomain(u".youtube.com"_s);
        QNetworkCookie g("x", "y");
        g.setDomain(u"accounts.google.com"_s);
        QNetworkCookie other("a", "b");
        other.setDomain(u"example.com"_s);
        QVERIFY(isYouTubeSessionCookie(yt));
        QVERIFY(isYouTubeSessionCookie(g));
        QVERIFY(!isYouTubeSessionCookie(other));
    }

    void formatsNetscapeLines()
    {
        QNetworkCookie c("SAPISID", "abc");
        c.setDomain(u".youtube.com"_s);
        c.setPath(u"/"_s);
        c.setSecure(true);
        c.setHttpOnly(true);
        c.setExpirationDate(QDateTime::fromSecsSinceEpoch(2000000000));
        const QString text = netscapeCookieText({c});
        QVERIFY(text.startsWith(u"# Netscape HTTP Cookie File"_s));
        QVERIFY(text.contains(u"#HttpOnly_.youtube.com\tTRUE\t/\tTRUE\t2000000000\tSAPISID\tabc\n"_s));
        QNetworkCookie session("s", "1");
        session.setDomain(u"www.youtube.com"_s);
        const QString sessionText = netscapeCookieText({session});
        QVERIFY(sessionText.contains(u"www.youtube.com\tFALSE\t/\tFALSE\t"_s));
        QVERIFY(!sessionText.contains(u"\t0\ts\t1"_s)); // session cookies get a future expiry
    }

    void writesPrivateTempFile()
    {
        QVERIFY(!writeCookiesTempFile({}));
        QNetworkCookie c("SID", "v");
        c.setDomain(u".youtube.com"_s);
        auto file = writeCookiesTempFile({c});
        QVERIFY(file);
        QVERIFY(QFile::exists(file->fileName()));
        QVERIFY(!(QFile::permissions(file->fileName()) & QFile::ReadGroup));
        QVERIFY(!(QFile::permissions(file->fileName()) & QFile::ReadOther));
        const QString name = file->fileName();
        file.reset();
        QVERIFY(!QFile::exists(name)); // removed with the object
    }
};

QTEST_GUILESS_MAIN(TestNetscapeCookies)
#include "tst_netscape_cookies.moc"
