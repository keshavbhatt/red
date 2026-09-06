#include "core/downloads/engine_spec.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestEngineSpec : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void assetsPerPlatform()
    {
        QCOMPARE(ytdlpComponent(u"linux"_s, u"x86_64"_s).assetName, u"yt-dlp_linux"_s);
        QCOMPARE(ytdlpComponent(u"linux"_s, u"arm64"_s).assetName, u"yt-dlp_linux_aarch64"_s);
        QCOMPARE(ytdlpComponent(u"windows"_s, u"x86_64"_s).installName, u"yt-dlp.exe"_s);
        QCOMPARE(jsRuntimeComponent(u"linux"_s, u"x86_64"_s).assetName, u"qjs-linux-x86_64"_s);
        QCOMPARE(jsRuntimeComponent(u"linux"_s, u"x86_64"_s).installName, u"qjs"_s);
        QVERIFY(!ffmpegInstallHint().isEmpty()); // ffmpeg is never downloaded, only hinted
    }

    void versionCompare()
    {
        QVERIFY(isNewerVersion(u"2026.08.19"_s, u"2026.03.13"_s));
        QVERIFY(!isNewerVersion(u"2026.03.13"_s, u"2026.08.19"_s));
        QVERIFY(!isNewerVersion(u"2026.08.19"_s, u"2026.08.19"_s));
        QVERIFY(isNewerVersion(u"2026.08.19.1"_s, u"2026.08.19"_s));
        QVERIFY(isNewerVersion(u"2026.09.01"_s, QString()));
        QVERIFY(!isNewerVersion(QString(), u"2026.09.01"_s));
        // Red v9 compared day-of-month alone: 2026.09.01 vs 2026.08.19 must be newer.
        QVERIFY(isNewerVersion(u"2026.09.01"_s, u"2026.08.19"_s));
    }

    void redirectUrls()
    {
        QCOMPARE(latestAssetUrl(u"yt-dlp/yt-dlp"_s, u"yt-dlp_linux"_s),
                 QUrl(u"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux"_s));
        QCOMPARE(latestReleaseUrl(u"quickjs-ng/quickjs"_s),
                 QUrl(u"https://github.com/quickjs-ng/quickjs/releases/latest"_s));
        QCOMPARE(tagFromReleaseUrl(
                     QUrl(u"https://github.com/yt-dlp/yt-dlp/releases/download/2026.08.19/yt-dlp_linux"_s)),
                 u"2026.08.19"_s);
        QCOMPARE(tagFromReleaseUrl(QUrl(u"https://github.com/quickjs-ng/quickjs/releases/tag/v0.16.2"_s)),
                 u"v0.16.2"_s);
        QCOMPARE(tagFromReleaseUrl(QUrl(u"https://github.com/yt-dlp/yt-dlp/releases/latest"_s)), QString());
        QCOMPARE(ytdlpComponent(u"linux"_s, u"x86_64"_s).repo, u"yt-dlp/yt-dlp"_s);
    }

    void releaseJsonAndSums()
    {
        const QByteArray json =
            "{\"tag_name\":\"2026.08.19\",\"assets\":[{\"name\":\"yt-dlp_linux\",\"browser_download_url\":"
            "\"https://x/yt-dlp_linux\"},{\"name\":\"SHA2-256SUMS\",\"browser_download_url\":\"https://x/"
            "sums\"}]}";
        QCOMPARE(releaseTagFromJson(json), u"2026.08.19"_s);
        QCOMPARE(assetUrlFromJson(json, u"yt-dlp_linux"_s), QUrl(u"https://x/yt-dlp_linux"_s));
        QVERIFY(assetUrlFromJson(json, u"missing"_s).isEmpty());
        const QByteArray sums =
            "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef  yt-dlp_linux\nffff  other\n";
        QCOMPARE(sha256FromSums(sums, u"yt-dlp_linux"_s),
                 u"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"_s);
        QVERIFY(sha256FromSums(sums, u"other"_s).isEmpty());
    }

    void labelsAndEnvironment()
    {
        QCOMPARE(jsRuntimeLabel(u"deno:/usr/bin/deno"_s), u"Deno"_s);
        QCOMPARE(jsRuntimeLabel(u"quickjs:/x/qjs"_s), u"QuickJS"_s);
        qputenv("LD_LIBRARY_PATH", "/snap/lib");
        qunsetenv("SNAP");
        QVERIFY(!engineProcessEnvironment().contains(u"LD_LIBRARY_PATH"_s));
        qputenv("SNAP", "/snap/red/1");
        QVERIFY(engineProcessEnvironment().contains(u"LD_LIBRARY_PATH"_s));
        qunsetenv("SNAP");
        qunsetenv("LD_LIBRARY_PATH");
    }
};

QTEST_GUILESS_MAIN(TestEngineSpec)
#include "tst_engine_spec.moc"
