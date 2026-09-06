#pragma once

#include <memory>

class QObject;

namespace red::core {
class INotifier;
}

namespace red::platform {

/// The native notification backend for this OS, or nullptr when none is
/// usable (the UI then falls back to tray balloons). Linux: portal inside
/// Flatpak, org.freedesktop.Notifications otherwise.
[[nodiscard]] std::unique_ptr<core::INotifier> createPlatformNotifier(QObject* parent = nullptr);

} // namespace red::platform
