#include "core/settings/settings.h"
#include "core/theme/theme_service.h"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using red::core::Settings;
using red::core::Theme;
using red::core::ThemeService;

class TestThemeService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void followsExplicitSetting()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        settings.setTheme(Theme::Dark);

        ThemeService service(settings);
        QCOMPARE(service.effectiveScheme(), Qt::ColorScheme::Dark);
        QVERIFY(service.isDark());

        QSignalSpy spy(&service, &ThemeService::effectiveSchemeChanged);
        settings.setTheme(Theme::Light);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(service.effectiveScheme(), Qt::ColorScheme::Light);

        settings.setTheme(Theme::Light); // no change → no signal
        QCOMPARE(spy.count(), 1);
    }

    void systemResolvesToPlatformOrLight()
    {
        QTemporaryDir dir;
        Settings settings(dir.filePath(u"s.ini"_s));
        settings.setTheme(Theme::System);
        ThemeService service(settings);

        const Qt::ColorScheme platform = QGuiApplication::styleHints()->colorScheme();
        const Qt::ColorScheme expected =
            platform == Qt::ColorScheme::Unknown ? Qt::ColorScheme::Light : platform;
        QCOMPARE(service.effectiveScheme(), expected);
    }

    void palettesDiffer()
    {
        QVERIFY(ThemeService::lightPalette().color(QPalette::Window) !=
                ThemeService::darkPalette().color(QPalette::Window));
        // Selection is Red's red (ADR-023), with white text on it.
        for (const QPalette& palette : {ThemeService::darkPalette(), ThemeService::lightPalette()}) {
            QCOMPARE(palette.color(QPalette::Highlight), QColor(0xFF, 0x00, 0x33));
            QCOMPARE(palette.color(QPalette::HighlightedText), QColor(Qt::white));
        }
    }
};

QTEST_MAIN(TestThemeService)
#include "tst_theme_service.moc"
