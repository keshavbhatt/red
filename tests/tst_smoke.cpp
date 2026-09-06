// Smoke test: the whole widget stack constructs, shows, and tears down
// offscreen without crashing. Runs under QT_QPA_PLATFORM=offscreen.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/main_window.h"

#include <QApplication>
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
        window.start(false);
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
        window.setTvMode(true);
        QTest::qWait(100);
        window.setTvMode(false);
        QTest::qWait(100);
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
