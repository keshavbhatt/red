#pragma once

#include <QObject>

#include <memory>

namespace red::platform {

/// Keeps the screen awake while a video plays (FEATURES D8). Linux talks to
/// org.freedesktop.ScreenSaver; elsewhere this is a no-op.
class ScreenInhibitor : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScreenInhibitor)

public:
    using QObject::QObject;
    ~ScreenInhibitor() override = default;

    [[nodiscard]] static std::unique_ptr<ScreenInhibitor> create(QObject* parent = nullptr);

    /// Idempotent; the inhibit is released when the object dies.
    virtual void setInhibited(bool inhibited, const QString& reason) = 0;
    [[nodiscard]] virtual bool isInhibited() const = 0;
};

} // namespace red::platform
