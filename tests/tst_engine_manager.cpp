// Network test, skipped unless RED_NETWORK_TESTS=1: provisions yt-dlp and
// QuickJS into a scratch data directory with PATH emptied so nothing on the
// host is found (ffmpeg is reported missing, never downloaded), then restores
// PATH and runs a real probe through the installed engine.

#include "core/downloads/media_info.h"
#include "core/settings/settings.h"
#include "services/engine_manager.h"
#include "services/media_probe.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestEngineManager : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void provisionsEverything()
    {
        if (!qEnvironmentVariableIsSet("RED_NETWORK_TESTS")) {
            QSKIP("set RED_NETWORK_TESTS=1 to download the engine (~45 MB)");
        }
        QTemporaryDir dir;
        red::core::Settings settings(dir.filePath(u"s.ini"_s));
        const QByteArray hostPath = qgetenv("PATH");
        qputenv("PATH", "/nonexistent");
        red::services::EngineManager engine(settings);
        engine.initialize();
        QVERIFY(!engine.isReady());
        QSignalSpy ready(&engine, &red::services::EngineManager::ready);
        QSignalSpy failed(&engine, &red::services::EngineManager::installFailed);
        engine.install();
        // yt-dlp + qjs arrive; ffmpeg is reported missing with an install hint.
        QTRY_VERIFY_WITH_TIMEOUT(ready.count() > 0 || failed.count() > 0, 600000);
        QCOMPARE(failed.count(), 1);
        QVERIFY(engine.status().ffmpegMissing);
        QVERIFY(engine.status().error.contains(u"ffmpeg"_s));
        QVERIFY(engine.status().ytdlpPath.endsWith(u"/engine/yt-dlp"_s));
        QVERIFY(engine.status().jsRuntime.startsWith(u"quickjs:"_s));
        QVERIFY(!engine.status().ytdlpVersion.isEmpty());
        // The user installs ffmpeg (here: the host PATH comes back) and re-checks.
        qputenv("PATH", hostPath);
        engine.initialize();
        QVERIFY(engine.isReady());
        const auto& s = engine.status();
        QVERIFY(!s.ffmpegPath.isEmpty());
        QVERIFY(
            !s.jsRuntime.isEmpty()); // the host deno is preferred once visible again (yt-dlp recommends it)

        red::services::MediaProbe probe;
        probe.setEnginePaths(engine.paths());
        QSignalSpy done(&probe, &red::services::MediaProbe::finished);
        QSignalSpy err(&probe, &red::services::MediaProbe::failed);
        probe.probe(QUrl(u"https://www.youtube.com/watch?v=jNQXAC9IVRw"_s), false);
        QTRY_VERIFY_WITH_TIMEOUT(done.count() > 0 || err.count() > 0, 120000);
        QCOMPARE(err.count(), 0);
        const auto info = done.first().at(1).value<red::core::MediaInfo>();
        QCOMPARE(info.title, u"Me at the zoo"_s);
        QVERIFY(info.formats.size() > 5);
    }
};

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    qputenv("QTEST_FUNCTION_TIMEOUT", "900000"); // the ffmpeg build is ~128 MB
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"red-engine-test"_s);
    QCoreApplication::setOrganizationName(u"ktechpit"_s);
    TestEngineManager test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_engine_manager.moc"
