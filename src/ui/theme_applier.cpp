#include "ui/theme_applier.h"

#include "core/theme/theme_service.h"
#include "ui/logging.h"
#include "ui/red_style.h"

#include <QApplication>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QStyleHints>

using namespace Qt::StringLiterals;

namespace red::ui {

ThemeApplier::ThemeApplier(core::ThemeService& theme, QObject* parent)
    : QObject(parent)
    , m_theme(theme)
{
    if (QStyle* fusion = QStyleFactory::create(u"Fusion"_s)) {
        QApplication::setStyle(fusion);
    } else {
        qCWarning(lcUi) << "Fusion style unavailable; using platform default";
    }
    // YouTube's face when it is installed; the system UI font otherwise.
    QFont font = QApplication::font();
    if (QFontDatabase::families().contains(u"Roboto"_s)) {
        font.setFamily(u"Roboto"_s);
    }
    if (font.pointSize() < 10) {
        font.setPointSize(10);
    }
    QApplication::setFont(font);
    apply();
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) { apply(); });
}

void ThemeApplier::apply()
{
    QApplication::setPalette(m_theme.palette());
    qApp->setStyleSheet(redStyleSheet(m_theme.isDark()));
    // The page follows theme-control.js (FEATURES D1); the Qt colour scheme is
    // still pushed so prefers-color-scheme agrees in "System" mode.
    QStyleHints* hints = QApplication::styleHints();
    if (m_theme.followsSystem()) {
        hints->unsetColorScheme();
    } else {
        hints->setColorScheme(m_theme.effectiveScheme());
    }
}

} // namespace red::ui
