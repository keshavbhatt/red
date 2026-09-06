#include "core/theme/theme_service.h"

#include "core/logging.h"
#include "core/settings/settings.h"

#include <QGuiApplication>
#include <QStyleHints>

namespace red::core {

namespace {

Qt::ColorScheme platformScheme()
{
    if (QGuiApplication::instance() == nullptr) {
        return Qt::ColorScheme::Light;
    }
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Unknown ? Qt::ColorScheme::Light : scheme;
}

Qt::ColorScheme resolve(Theme theme)
{
    switch (theme) {
    case Theme::Light:
        return Qt::ColorScheme::Light;
    case Theme::Dark:
        return Qt::ColorScheme::Dark;
    case Theme::System:
        break;
    }
    return platformScheme();
}

} // namespace

ThemeService::ThemeService(Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_current(resolve(settings.theme()))
{
    connect(&m_settings, &Settings::themeChanged, this, [this](Theme) { reevaluate(); });
    if (QGuiApplication::instance() != nullptr) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
                [this](Qt::ColorScheme) {
                    if (followsSystem()) {
                        reevaluate();
                    }
                });
    }
}

Qt::ColorScheme ThemeService::effectiveScheme() const
{
    return m_current;
}

bool ThemeService::followsSystem() const
{
    return m_settings.theme() == Theme::System;
}

void ThemeService::reevaluate()
{
    const Qt::ColorScheme next = resolve(m_settings.theme());
    if (next == m_current) {
        return;
    }
    m_current = next;
    qCInfo(lcCore) << "effective colour scheme:" << (isDark() ? "dark" : "light");
    Q_EMIT effectiveSchemeChanged(next);
}

QPalette ThemeService::lightPalette()
{
    // Red's brand tokens (DESIGN.md §1), light: YouTube's white surfaces.
    QPalette p;
    p.setColor(QPalette::Window, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::WindowText, QColor(0x0F, 0x0F, 0x0F));
    p.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::AlternateBase, QColor(0xF9, 0xF9, 0xF9));
    p.setColor(QPalette::ToolTipBase, QColor(0xFF, 0xFF, 0xFF));
    p.setColor(QPalette::ToolTipText, QColor(0x0F, 0x0F, 0x0F));
    p.setColor(QPalette::Text, QColor(0x0F, 0x0F, 0x0F));
    p.setColor(QPalette::Button, QColor(0xF2, 0xF2, 0xF2));
    p.setColor(QPalette::ButtonText, QColor(0x0F, 0x0F, 0x0F));
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x06, 0x5F, 0xD4));
    p.setColor(QPalette::Highlight, QColor(0xFF, 0x00, 0x00));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0x60, 0x60, 0x60));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x90, 0x90, 0x90));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x90, 0x90, 0x90));
    return p;
}

QPalette ThemeService::darkPalette()
{
    // Red's brand tokens (DESIGN.md §1), dark: YouTube's #0F0F0F surfaces.
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x0F, 0x0F, 0x0F));
    p.setColor(QPalette::WindowText, QColor(0xF1, 0xF1, 0xF1));
    p.setColor(QPalette::Base, QColor(0x12, 0x12, 0x12));
    p.setColor(QPalette::AlternateBase, QColor(0x1F, 0x1F, 0x1F));
    p.setColor(QPalette::ToolTipBase, QColor(0x28, 0x28, 0x28));
    p.setColor(QPalette::ToolTipText, QColor(0xF1, 0xF1, 0xF1));
    p.setColor(QPalette::Text, QColor(0xF1, 0xF1, 0xF1));
    p.setColor(QPalette::Button, QColor(0x27, 0x27, 0x27));
    p.setColor(QPalette::ButtonText, QColor(0xF1, 0xF1, 0xF1));
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x3E, 0xA6, 0xFF));
    p.setColor(QPalette::Highlight, QColor(0xFF, 0x00, 0x00));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, QColor(0xAA, 0xAA, 0xAA));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x71, 0x71, 0x71));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x71, 0x71, 0x71));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x71, 0x71, 0x71));
    return p;
}

} // namespace red::core
