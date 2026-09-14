#pragma once

#include <QObject>

#include <memory>

namespace red::platform {

/// The progress bar a task manager can draw behind the app's entry (Plasma,
/// Unity-style launchers), used for playback position. Linux emits the
/// com.canonical.Unity.LauncherEntry signal; elsewhere this is a no-op.
class TaskbarProgress : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TaskbarProgress)

public:
    using QObject::QObject;
    ~TaskbarProgress() override = default;

    /// `desktopEntry` is the desktop file id (with or without ".desktop").
    [[nodiscard]] static std::unique_ptr<TaskbarProgress> create(const QString& desktopEntry,
                                                                 QObject* parent = nullptr);

    /// 0..1; hidden again with clear(). Cheap when nothing changed.
    virtual void setProgress(double fraction) = 0;
    virtual void clear() = 0;
};

} // namespace red::platform
