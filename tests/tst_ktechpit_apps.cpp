// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#include "AppCatalog.h"
#include "AppLinks.h"

#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <QTest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using ktechpit::AppCatalog;
using ktechpit::AppInfo;
using ktechpit::Package;

namespace {

AppInfo app(const QString& id)
{
    AppInfo a;
    a.id = id;
    a.name = id;
    return a;
}

QList<AppInfo> pool()
{
    return {app(u"wallpaper"_s), app(u"games"_s), app(u"dict"_s), app(u"paraphrase"_s), app(u"glate"_s)};
}

QStringList ids(const QList<AppInfo>& apps)
{
    QStringList out;
    for (const AppInfo& a : apps) {
        out << a.id;
    }
    return out;
}

} // namespace

class TestKtechpitApps : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesSchemaOne()
    {
        // Plain literals: moc reads "//" inside a raw string as a comment.
        const QByteArray json =
            "{\"schema\":1,\"apps\":["
            "{\"id\":\"glate\",\"name\":\"Glate\",\"summary\":\"Translate\","
            "\"tags\":[\"language\",\"text\"],\"trending\":2,\"snap\":\"glate\","
            "\"flatpak\":\"com.ktechpit.glate\",\"platforms\":[\"Linux\"]},"
            "{\"id\":\"\",\"name\":\"no id\"},"
            "{\"id\":\"x\",\"name\":\"X\",\"trending\":null,\"flatpak\":null}]}";
        bool ok = false;
        const QList<AppInfo> apps = AppCatalog::parse(json, &ok);
        QVERIFY(ok);
        QCOMPARE(apps.size(), 2);
        QCOMPARE(apps.at(0).platforms, QStringList({u"Linux"_s}));
        QCOMPARE(apps.at(0).flatpak, u"com.ktechpit.glate"_s);
        QVERIFY(apps.at(1).flatpak.isEmpty());
    }

    void refusesOtherSchemas()
    {
        bool ok = true;
        QVERIFY(AppCatalog::parse(R"({"schema":2,"apps":[{"id":"a","name":"A"}]})", &ok).isEmpty());
        QVERIFY(!ok);
        QVERIFY(AppCatalog::parse("not json", &ok).isEmpty());
        QVERIFY(!ok);
    }

    void bundledCatalogParses()
    {
        AppCatalog::init({}); // registers the resources
        QFile file(u":/ktechpit-apps/catalog.json"_s);
        QVERIFY(file.open(QIODevice::ReadOnly));
        bool ok = false;
        const QList<AppInfo> apps = AppCatalog::parse(file.readAll(), &ok);
        QVERIFY(ok);
        QVERIFY(apps.size() >= 10);
        for (const AppInfo& a : apps) {
            QVERIFY2(QFile::exists(u":/ktechpit-apps/icons/"_s + a.id + u".png"_s), qPrintable(a.id));
        }
    }

    void randomAppsVaryAndNeverRepeat()
    {
        const QList<AppInfo> apps = pool();
        QSet<QString> seen;
        QSet<QString> firsts;
        for (quint32 seed = 1; seed <= 200; ++seed) {
            QRandomGenerator random(seed);
            const QStringList got = ids(AppCatalog::randomApps(apps, 2, random));
            QCOMPARE(got.size(), 2);
            QCOMPARE(QSet<QString>(got.begin(), got.end()).size(), 2); // no app twice
            firsts.insert(got.first());
            for (const QString& id : got) {
                seen.insert(id);
            }
        }
        QCOMPARE(seen.size(), apps.size());   // every app comes up
        QCOMPARE(firsts.size(), apps.size()); // and any of them can lead
    }

    void randomAppsStopWhenThePoolRunsOut()
    {
        QRandomGenerator random(3);
        QCOMPARE(AppCatalog::randomApps(pool(), 50, random).size(), pool().size());
        QVERIFY(AppCatalog::randomApps(pool(), 0, random).isEmpty());
        QVERIFY(AppCatalog::randomApps({}, 3, random).isEmpty());
    }

    void getLinkFollowsThePackage()
    {
        AppInfo a = app(u"glate"_s);
        a.snap = u"glate"_s;
        a.flatpak = u"com.ktechpit.glate"_s;
        a.page = QUrl(u"https://ktechpit.com/USS/public/product.php?slug=glate"_s);

        QCOMPARE(ktechpit::AppLinks::getUrl(a, Package::Snap, u"red"_s, u"about"_s), QUrl(u"snap://glate"_s));
        QCOMPARE(ktechpit::AppLinks::getUrl(a, Package::Flatpak, u"red"_s, u"about"_s),
                 QUrl(u"appstream://com.ktechpit.glate"_s));
        QCOMPARE(ktechpit::AppLinks::getUrl(a, Package::Native, u"red"_s, u"about"_s),
                 QUrl(u"appstream://com.ktechpit.glate"_s));

        const QUrl web = ktechpit::AppLinks::getUrl(a, Package::AppImage, u"red"_s, u"about"_s);
        QCOMPARE(web.host(), u"ktechpit.com"_s);
        QCOMPARE(QUrlQuery(web).queryItemValue(u"slug"_s), u"glate"_s);
        QCOMPARE(QUrlQuery(web).queryItemValue(u"ref"_s), u"red-about"_s);

        // Not on Flathub: a Flatpak user gets the web page, a native user the snap.
        a.flatpak.clear();
        QCOMPARE(ktechpit::AppLinks::getUrl(a, Package::Flatpak, u"red"_s, u"about"_s).scheme(), u"https"_s);
        QCOMPARE(ktechpit::AppLinks::getUrl(a, Package::Native, u"red"_s, u"about"_s), QUrl(u"snap://glate"_s));
    }

    void pageUrlWithoutSource()
    {
        AppInfo a = app(u"red"_s);
        a.page = QUrl(u"https://ktechpit.com/USS/public/product.php?slug=red"_s);
        QCOMPARE(ktechpit::AppLinks::pageUrl(a, QString(), u"about"_s), a.page);
    }
};

QTEST_MAIN(TestKtechpitApps)
#include "tst_ktechpit_apps.moc"
