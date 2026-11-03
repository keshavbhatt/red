#include "core/settings/settings.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"red.ini"_s));
    }

    void defaults()
    {
        QCOMPARE(m_settings->theme(), Theme::System);
        QCOMPARE(m_settings->closeAction(), CloseAction::Quit);
        QCOMPARE(m_settings->startPage(), StartPage::Home);
        QVERIFY(m_settings->blockAds());
        QVERIFY(m_settings->blockTrackers());
        QVERIFY(m_settings->sponsorBlockEnabled());
        QVERIFY(m_settings->showDislikes());
        QVERIFY(m_settings->autoplay());
        QCOMPARE(m_settings->codecPreference(), CodecPreference::Auto);
        QVERIFY(!m_settings->hardwareVideoDecode());
        QCOMPARE(m_settings->concurrentDownloads(), 2);
        QCOMPARE(m_settings->speedLimitKbps(), 0);
        QVERIFY(m_settings->useSessionCookies());
        QVERIFY(m_settings->downloadDirectory().endsWith(u"/Red"_s));
        QVERIFY(m_settings->whatsNewSeenVersion().isEmpty());
        m_settings->setWhatsNewSeenVersion(u" 10.0.0 "_s);
        QCOMPARE(m_settings->whatsNewSeenVersion(), u"10.0.0"_s);
        QVERIFY(m_settings->organiseDownloads());
        m_settings->setOrganiseDownloads(false);
        QVERIFY(!m_settings->organiseDownloads());
        QVERIFY(!m_settings->taskbarProgress());
        m_settings->setTaskbarProgress(true);
        QVERIFY(m_settings->taskbarProgress());
        QVERIFY(m_settings->freeDownloads().day.isEmpty());
        m_settings->setFreeDownloads({u"2026-09-13"_s, 3});
        QCOMPARE(m_settings->freeDownloads().used, 3);
        m_settings->resetToDefaults(); // the counter is not a preference
        QCOMPARE(m_settings->freeDownloads().day, u"2026-09-13"_s);
        QCOMPARE(m_settings->freeDownloads().used, 3);
        QCOMPARE(m_settings->defaultContainer(), Container::Mp4);
        QVERIFY(m_settings->tvFullscreen());
        QVERIFY(m_settings->tvGamepad());
        QCOMPARE(m_settings->lastMode(), AppMode::Desktop);
        QVERIFY(m_settings->railLabels());         // new users see what each rail button is
        QVERIFY(!m_settings->middleClickScroll()); // Linux pastes with the middle button
        QVERIFY(m_settings->swipeNavigation());    // as in a browser
        QSignalSpy labels(m_settings.get(), &Settings::railLabelsChanged);
        m_settings->setRailLabels(false);
        m_settings->setRailLabels(false);
        QCOMPARE(labels.count(), 1);
        QVERIFY(!m_settings->railLabels());
        QCOMPARE(m_settings->zoomFactor(), 1.0);
        QCOMPARE(m_settings->sponsorAction(u"sponsor"_s), SponsorAction::Skip);
        QCOMPARE(m_settings->sponsorAction(u"filler"_s), SponsorAction::Off);
        QCOMPARE(m_settings->sponsorAction(u"unknown"_s), SponsorAction::Off);
    }

    void signalsAndClamping()
    {
        QSignalSpy theme(m_settings.get(), &Settings::themeChanged);
        m_settings->setTheme(Theme::Dark);
        m_settings->setTheme(Theme::Dark);
        QCOMPARE(theme.count(), 1);
        QCOMPARE(m_settings->theme(), Theme::Dark);

        QSignalSpy zoom(m_settings.get(), &Settings::zoomFactorChanged);
        m_settings->setZoomFactor(99);
        QCOMPARE(m_settings->zoomFactor(), 5.0);
        QCOMPARE(zoom.count(), 1);

        m_settings->setConcurrentDownloads(50);
        QCOMPARE(m_settings->concurrentDownloads(), Settings::kMaxConcurrentDownloads);
        m_settings->setSpeedLimitKbps(-5);
        QCOMPARE(m_settings->speedLimitKbps(), 0);

        QSignalSpy sponsor(m_settings.get(), &Settings::sponsorBlockChanged);
        m_settings->setSponsorAction(u"intro"_s, SponsorAction::Skip);
        QCOMPARE(m_settings->sponsorAction(u"intro"_s), SponsorAction::Skip);
        QCOMPARE(sponsor.count(), 1);
        QCOMPARE(m_settings->sponsorActions().value(u"intro"_s), SponsorAction::Skip);

        QSignalSpy blocking(m_settings.get(), &Settings::blockingChanged);
        m_settings->setBlockAds(false);
        m_settings->setBlockTrackers(false);
        QCOMPARE(blocking.count(), 2);
    }

    void gpuFallbackResetsOnExplicitChoice()
    {
        m_settings->setGpuAutoDisabled(true);
        m_settings->setGpuProbeStrikes(1);
        m_settings->setHardwareAcceleration(HardwareAcceleration::On);
        QVERIFY(!m_settings->gpuAutoDisabled());
        QCOMPARE(m_settings->gpuProbeStrikes(), 0);
    }

    void persistsAcrossInstances()
    {
        m_settings->setDownloadDirectory(u"/tmp/red-test"_s);
        m_settings->setLastMode(AppMode::Tv);
        m_settings->setSubtitleLanguages({u"en"_s, u"de"_s});
        m_settings->sync();
        Settings again(m_dir->filePath(u"red.ini"_s));
        QCOMPARE(again.downloadDirectory(), u"/tmp/red-test"_s);
        QCOMPARE(again.lastMode(), AppMode::Tv);
        QCOMPARE(again.subtitleLanguages(), (QStringList{u"en"_s, u"de"_s}));
    }

    void startModeDefaultsToYouTube()
    {
        // A mode left open last time no longer decides the next launch.
        m_settings->setLastMode(AppMode::Tv);
        QCOMPARE(m_settings->startMode(), StartMode::Desktop);
        QCOMPARE(m_settings->startupMode(), AppMode::Desktop);
        m_settings->setStartMode(StartMode::LastUsed);
        QCOMPARE(m_settings->startupMode(), AppMode::Tv);
        m_settings->setStartMode(StartMode::Music);
        QCOMPARE(m_settings->startupMode(), AppMode::Music);
        QSignalSpy changed(m_settings.get(), &Settings::startModeChanged);
        m_settings->setStartMode(StartMode::Music);
        QCOMPARE(changed.count(), 0);
        m_settings->resetToDefaults();
        QCOMPARE(m_settings->startMode(), StartMode::Desktop);
    }

    void firstRunUntilWelcomeOrNotes()
    {
        QVERIFY(m_settings->isFirstRun());
        m_settings->setWelcomeSeen(true);
        QVERIFY(!m_settings->isFirstRun());
        m_settings->setWelcomeSeen(false);
        m_settings->setWhatsNewSeenVersion(u"10.2.0"_s); // an upgrade from a Red 10 with release notes
        QVERIFY(!m_settings->isFirstRun());
    }

    void generalGroupSurvivesARestart()
    {
        // QSettings' INI format reads a "[general]" section as its special
        // "[General]" (keys without a group): the general/ keys must still
        // come back after a restart.
        m_settings->setStartMode(StartMode::Music);
        m_settings->setStartPage(StartPage::Subscriptions);
        m_settings->setMiddleClickScroll(true);
        m_settings->setSwipeNavigation(false);
        m_settings->setNotifyOnDownloadFinish(false);
        m_settings->sync();
        Settings again(m_dir->filePath(u"red.ini"_s));
        QCOMPARE(again.startMode(), StartMode::Music);
        QCOMPARE(again.startPage(), StartPage::Subscriptions);
        QVERIFY(again.middleClickScroll());
        QVERIFY(!again.swipeNavigation());
        QVERIFY(!again.notifyOnDownloadFinish());
    }

    void generalGroupReadFromDisk()
    {
        // What 10.2 and earlier left on disk for Settings, General: migrated,
        // so the user's choices come back (a fresh file, no in-process cache).
        const QString path = m_dir->filePath(u"written-earlier.ini"_s);
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[%General]\nmiddleClickScroll=true\nstartPage=1\n");
        }
        Settings fresh(path);
        QCOMPARE(fresh.startPage(), StartPage::Subscriptions);
        QVERIFY(fresh.middleClickScroll());
    }

    void appGroupReadFromDisk()
    {
        // The app/ keys written to disk come back in a fresh store.
        const QString path = m_dir->filePath(u"written-now.ini"_s);
        {
            Settings writer(path);
            writer.setStartMode(StartMode::Tv);
            writer.setTrayEnabled(false);
            writer.sync();
        }
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray text = f.readAll();
        QVERIFY(text.contains("[app]"));
        QVERIFY(!text.contains("General"));
    }

    void startModeMigratesStartInTv()
    {
        const QString path = m_dir->filePath(u"legacy.ini"_s);
        {
            QSettings legacy(path, QSettings::IniFormat);
            legacy.setValue(u"tv/startInTvMode"_s, true);
        }
        Settings settings(path);
        QCOMPARE(settings.startMode(), StartMode::Tv);
        QCOMPARE(settings.startupMode(), AppMode::Tv);
        settings.setStartMode(StartMode::Desktop); // the old key goes with the first choice
        settings.sync();
        QSettings after(path, QSettings::IniFormat);
        QVERIFY(!after.contains(u"tv/startInTvMode"_s));
        QCOMPARE(Settings(path).startMode(), StartMode::Desktop);
    }

    void musicModeRoundTrip()
    {
        m_settings->setLastMode(AppMode::Music);
        m_settings->setLastMusicUrl(u"https://music.youtube.com/library"_s);
        m_settings->sync();
        Settings again(m_dir->filePath(u"red.ini"_s));
        QCOMPARE(again.lastMode(), AppMode::Music);
        QCOMPARE(again.lastMusicUrl(), u"https://music.youtube.com/library"_s);
        QCOMPARE(QString(appModeName(AppMode::Music)), u"music"_s);
        QCOMPARE(QString(appModeName(AppMode::Desktop)), u"desktop"_s);
    }

    void resetToDefaults()
    {
        m_settings->setTheme(Theme::Light);
        m_settings->setBlockAds(false);
        QSignalSpy theme(m_settings.get(), &Settings::themeChanged);
        m_settings->resetToDefaults();
        QCOMPARE(m_settings->theme(), Theme::System);
        QVERIFY(m_settings->blockAds());
        QVERIFY(theme.count() >= 1);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
