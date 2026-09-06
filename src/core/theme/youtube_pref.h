#pragma once

#include "core/settings/settings.h"

#include <QString>

// YouTube stores its own theme choice in the PREF cookie's `f6` bit field
// (verified 2026-09-06 over CDP: f6=80000 renders light, f6=400 renders
// dark, neither bit = follow the device). Setting the bit before a page load
// makes YouTube itself render the theme — no fighting the DOM afterwards.
namespace red::core {

inline constexpr quint64 kYouTubePrefDarkBit = 0x400;
inline constexpr quint64 kYouTubePrefLightBit = 0x80000;

/// The PREF cookie value with the theme bits set for `theme`, keeping every
/// other field ("tz=…&f7=…") intact. Works from an empty value, too.
[[nodiscard]] QString youTubePrefForTheme(const QString& prefValue, Theme theme);

/// The theme the PREF value currently encodes (System when neither bit is set).
[[nodiscard]] Theme themeFromYouTubePref(const QString& prefValue);

} // namespace red::core
