#include "services/netscape_cookies.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace red::services;

class TestNetscapeCookies : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsChromiumDatabase()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(u"Cookies"_s);
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"fixture"_s);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec(
                u"CREATE TABLE cookies (creation_utc INTEGER, host_key TEXT, top_frame_site_key TEXT, "
                "name TEXT, value TEXT, encrypted_value BLOB, path TEXT, expires_utc INTEGER, "
                "is_secure INTEGER, is_httponly INTEGER, last_access_utc INTEGER, has_expires INTEGER, "
                "is_persistent INTEGER, priority INTEGER, samesite INTEGER, source_scheme INTEGER, "
                "source_port INTEGER, last_update_utc INTEGER, source_type INTEGER, "
                "has_cross_site_ancestor INTEGER)"_s));
            // 13467811745821431 µs since 1601 = 2027-09-08 UTC; 0 = session cookie.
            QVERIFY(q.exec(
                u"INSERT INTO cookies (host_key,name,value,path,expires_utc,is_secure,is_httponly) VALUES "
                "('.youtube.com','SAPISID','abc','/',13467811745821431,1,0),"
                "('.youtube.com','YSC','s','/',0,1,1),"
                "('.example.com','x','y','/',0,0,0)"_s));
            db.close();
        }
        QSqlDatabase::removeDatabase(u"fixture"_s);
        const QList<QNetworkCookie> cookies = readChromiumCookieDatabase(path);
        QCOMPARE(cookies.size(), 3);
        const auto sapisid = std::find_if(cookies.cbegin(), cookies.cend(),
                                          [](const QNetworkCookie& c) { return c.name() == "SAPISID"; });
        QVERIFY(sapisid != cookies.cend());
        QCOMPARE(sapisid->domain(), u".youtube.com"_s);
        QCOMPARE(sapisid->value(), QByteArray("abc"));
        QVERIFY(sapisid->isSecure());
        QCOMPARE(sapisid->expirationDate().date().year(), 2027);
        const auto ysc = std::find_if(cookies.cbegin(), cookies.cend(),
                                      [](const QNetworkCookie& c) { return c.name() == "YSC"; });
        QVERIFY(ysc->isSessionCookie());
        QVERIFY(ysc->isHttpOnly());
        QVERIFY(readChromiumCookieDatabase(dir.filePath(u"missing"_s)).isEmpty());
    }

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
