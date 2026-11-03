// Smoke test: the whole widget stack constructs, shows, and tears down
// offscreen without crashing. Runs under QT_QPA_PLATFORM=offscreen.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/about_dialog.h"
#include "ui/main_window.h"
#include "ui/whats_new_dialog.h"

#include <QApplication>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestSmoke : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mainWindowConstructsAndShows()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        red::core::Settings settings(dir.filePath(u"smoke.ini"_s));
        red::core::ThemeService theme(settings);

        red::ui::MainWindow window(settings, theme, u"10.0.0-test"_s);
        window.start(std::nullopt);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(300);

        settings.setTheme(red::core::Theme::Dark);
        settings.setTheme(red::core::Theme::Light);
        settings.setZoomFactor(1.25);
        settings.setBlockAds(false);
        settings.setBlockAds(true);
        settings.setShowRail(false);
        settings.setShowRail(true);
        window.showAndRaise();
        window.setMode(red::core::AppMode::Tv);
        QTest::qWait(100);
        window.setMode(red::core::AppMode::Desktop);
        QTest::qWait(100);
        window.close();
    }

    // About's "What's new" link closes About and shows the release notes on demand.
    void aboutOpensWhatsNew()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        red::core::Settings settings(dir.filePath(u"about.ini"_s));
        settings.setWelcomeSeen(true);
        settings.setWhatsNewSeenVersion(u"10.3.1"_s); // no automatic notes in the way
        red::core::ThemeService theme(settings);
        red::ui::MainWindow window(settings, theme, u"10.3.1"_s);
        window.start(std::nullopt);
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        window.showAbout();
        QPointer<red::ui::AboutDialog> about = window.findChild<red::ui::AboutDialog*>();
        QVERIFY(about);
        QPushButton* link = nullptr;
        for (QPushButton* b : about->findChildren<QPushButton*>()) {
            if (b->text() == u"What's new"_s) {
                link = b;
            }
        }
        QVERIFY(link != nullptr);
        QVERIFY(!link->autoDefault()); // Enter in About must not open it
        link->click();
        QTRY_VERIFY(window.findChild<red::ui::WhatsNewDialog*>() != nullptr);
        QTRY_VERIFY(about.isNull() || !about->isVisible());
        // It opens on the running version and browses older ones.
        auto* notes = window.findChild<red::ui::WhatsNewDialog*>();
        QCOMPARE(notes->shownVersion(), u"10.3.1"_s);
        notes->showVersion(u"10.0.0"_s);
        QCOMPARE(notes->shownVersion(), u"10.0.0"_s);
        notes->showVersion(u"9.9.9"_s); // not in the changelog: ignored
        QCOMPARE(notes->shownVersion(), u"10.0.0"_s);
        window.close();
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"red-smoke"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    QApplication::setQuitOnLastWindowClosed(false);
    TestSmoke test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_smoke.moc"
