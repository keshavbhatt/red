#pragma once

#include <QString>
#include <QStringList>

namespace red::core {

enum class HardwareAcceleration;

/// True when the GPU should be off: the setting is Off, or it is Auto and the
/// GPU was auto-disabled after an unstable trial (ADR-000 / W-ADR-032).
[[nodiscard]] bool useSoftwareGpu(HardwareAcceleration acceleration, bool autoDisabled);

/// Chromium switches derived from settings. Applied to
/// QTWEBENGINE_CHROMIUM_FLAGS before the web engine starts; a user-provided
/// value of that variable is kept and ours are merged in.
///  * hardwareVideoDecode — FEATURES D5: opt-in VA-API decode (Linux); off
///    keeps Chromium's CPU decode, which is the stable default on Linux.
[[nodiscard]] QStringList chromiumFlags(HardwareAcceleration acceleration, bool gpuAutoDisabled = false,
                                        bool hardwareVideoDecode = false);

/// Merges `userFlags` (existing env var content) with ours, deduplicated.
[[nodiscard]] QString mergeChromiumFlags(const QString& userFlags, const QStringList& ours);

} // namespace red::core
